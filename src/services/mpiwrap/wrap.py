#!/usr/bin/env python
#################################################################################################
# Copyright (c) 2010, Lawrence Livermore National Security, LLC.
# Produced at the Lawrence Livermore National Laboratory
# Written by Todd Gamblin, tgamblin@llnl.gov.
# LLNL-CODE-417602
# All rights reserved.
#
# This file is part of Libra. For details, see http://github.com/tgamblin/libra.
# Please also read the LICENSE file for further information.
#
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
#
#  * Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the disclaimer below.
#  * Redistributions in binary form must reproduce the above copyright notice, this list of
#    conditions and the disclaimer (as noted below) in the documentation and/or other materials
#    provided with the distribution.
#  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
#    or promote products derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#################################################################################################
from __future__ import print_function
usage_string = \
'''Usage: wrap.py [-fgd] [-i pmpi_init] [-c mpicc_name] [-o file] wrapper.w [...]
 Python script for creating PMPI wrappers. Roughly follows the syntax of
   the Argonne PMPI wrapper generator, with some enhancements.
 Options:"
   -d             Just dump function declarations parsed out of mpi.h
   -f             Generate fortran wrappers in addition to C wrappers.
   -g             Generate reentry guards around wrapper functions.
   -s             Skip writing #includes, #defines, and other front-matter (for non-C output).
   -c exe         Provide name of MPI compiler (for parsing mpi.h).  Default is \'mpicc\'.
   -I dir         Provide an extra include directory to use when parsing mpi.h.
   -i pmpi_init   Specify proper binding for the fortran pmpi_init function.
                  Default is \'pmpi_init_\'.  Wrappers compiled for PIC will guess the
                  right binding automatically (use -DPIC when you compile dynamic libs).
   -o file        Send output to a file instead of stdout.

 by Todd Gamblin, tgamblin@llnl.gov
'''
import tempfile, getopt, subprocess, sys, os, re, types, itertools

# Default values for command-line parameters
mpicc = 'mpicc'                    # Default name for the MPI compiler
includes = []                      # Default set of directories to inlucde when parsing mpi.h
pmpi_init_binding = "pmpi_init_"   # Default binding for pmpi_init
output_fortran_wrappers = False    # Don't print fortran wrappers by default
output_guards = False              # Don't print reentry guards by default
skip_headers = False               # Skip header information and defines (for non-C output)
dump_prototypes = False            # Just exit and dump MPI protos if false.

# Possible legal bindings for the fortran version of PMPI_Init()
pmpi_init_bindings = ["PMPI_INIT", "pmpi_init", "pmpi_init_", "pmpi_init__"]

# Possible function return types to consider, used for declaration parser.
# In general, all MPI calls we care about return int.  We include double
# to grab MPI_Wtick and MPI_Wtime, but we'll ignore the f2c and c2f calls
# that return MPI_Datatypes and other such things.
# MPI_Aint_add and MPI_Aint_diff return MPI_Aint, so include that too. 
rtypes = ['int', 'double', 'MPI_Aint' ]

# If we find these strings in a declaration, exclude it from consideration.
exclude_strings = [ "c2f", "f2c", "typedef" ]

# Regular expressions for start and end of declarations in mpi.h. These are
# used to get the declaration strings out for parsing with formal_re below.
begin_decl_re = re.compile("(" + "|".join(rtypes) + ")\s+(MPI_\w+)\s*\(")
exclude_re =    re.compile("|".join(exclude_strings))
end_decl_re =   re.compile("\).*\;")

# Regular Expression for splitting up args. Matching against this
# returns three groups: type info, arg name, and array info
formal_re = re.compile(
    "\s*(" +                       # Start type
    "(?:const)?\s*" +              # Initial const
    "\w+"                          # Type name (note: doesn't handle 'long long', etc. right now)
    ")\s*(" +                      # End type, begin pointers
    "(?:\s*\*(?:\s*const)?)*" +    # Look for 0 or more pointers with optional 'const'
    ")\s*"                         # End pointers
    "(?:(\w+)\s*)?" +              # Argument name. Optional.
     "(\[.*\])?\s*$"               # Array type.  Also optional. Works for multidimensions b/c it's greedy.
    )

# Fortran wrapper suffix
f_wrap_suffix = "_fortran_wrapper"

# Initial includes and defines for wrapper files.
wrapper_includes = '''
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _EXTERN_C_
#ifdef __cplusplus
#define _EXTERN_C_ extern "C"
#else /* __cplusplus */
#define _EXTERN_C_
#endif /* __cplusplus */
#endif /* _EXTERN_C_ */

#ifdef MPICH_HAS_C2F
_EXTERN_C_ void *MPIR_ToPointer(int);
#endif // MPICH_HAS_C2F

#ifdef PIC
/* For shared libraries, declare these weak and figure out which one was linked
   based on which init wrapper was called.  See mpi_init wrappers.  */
#pragma weak pmpi_init
#pragma weak PMPI_INIT
#pragma weak pmpi_init_
#pragma weak pmpi_init__
#endif /* PIC */

_EXTERN_C_ void pmpi_init(MPI_Fint *ierr);
_EXTERN_C_ void PMPI_INIT(MPI_Fint *ierr);
_EXTERN_C_ void pmpi_init_(MPI_Fint *ierr);
_EXTERN_C_ void pmpi_init__(MPI_Fint *ierr);

'''

# Default modifiers for generated bindings
default_modifiers = ["_EXTERN_C_"]  # _EXTERN_C_ is #defined (or not) in wrapper_includes. See above.

# Set of MPI Handle types
mpi_handle_types = set(["MPI_Comm", "MPI_Errhandler", "MPI_File", "MPI_Group", "MPI_Info",
                        "MPI_Op", "MPI_Request", "MPI_Status", "MPI_Datatype", "MPI_Win" ])

# MPI Calls that have array parameters, and mappings from the array parameter positions to the position
# of the 'count' paramters that determine their size
mpi_array_calls = {
    "MPI_Startall"           : { 1:0 },
    "MPI_Testall"            : { 1:0, 3:0 },
    "MPI_Testany"            : { 1:0 },
    "MPI_Testsome"           : { 1:0, 4:0 },
    "MPI_Type_create_struct" : { 3:0 },
    "MPI_Type_get_contents"  : { 6:1 },
    "MPI_Type_struct"        : { 3:0 },
    "MPI_Waitall"            : { 1:0, 2:0 },
    "MPI_Waitany"            : { 1:0 },
    "MPI_Waitsome"           : { 1:0, 4:0 }
}


def find_matching_paren(string, index, lparen='(', rparen=')'):
    """Find the closing paren corresponding to the open paren at <index>
       in <string>.  Optionally, can provide other characters to match on.
       If found, returns the index of the matching parenthesis.  If not found,
       returns -1.
    """
    if not string[index] == lparen:
        raise ValueError("Character at index %d is '%s'. Expected '%s'"
                         % (index, string[index], lparen))
    index += 1
    count = 1
    while index < len(string) and count > 0:
        while index < len(string) and string[index] not in (lparen, rparen):
            index += 1
        if string[index] == lparen:
            count += 1
        elif string[index] == rparen:
            count -= 1

    if count == 0:
        return index
    else:
        return -1


def isindex(str):
    """True if a string is something we can index an array with."""
    try:
        int(str)
        return True
    except ValueError:
        return False

def once(function):
    if not hasattr(function, "did_once"):
        function()
        function.did_once = True

# Returns MPI_Blah_[f2c,c2f] prefix for a handle type.  MPI_Datatype is a special case.
def conversion_prefix(handle_type):
    if handle_type == "MPI_Datatype":
        return "MPI_Type"
    else:
        return handle_type

# Special join function for joining lines together.  Puts "\n" at the end too.
def joinlines(list, sep="\n"):
    if list:
        return sep.join(list) + sep
    else:
        return ""

# Possible types of Tokens in input.
LBRACE, RBRACE, TEXT, IDENTIFIER = range(4)

class Token:
    """Represents tokens; generated from input by lexer and fed to parse()."""
    def __init__(self, type, value, line=0):
        self.type = type    # Type of token
        self.value = value  # Text value
        self.line = line

    def __str__(self):
        return "'%s'" % re.sub(r'\n', "\\\\n", self.value)

    def isa(self, type):
        return self.type == type


class LineTrackingLexer(object):
    """Base class for Lexers that keep track of line numbers."""
    def __init__(self, lexicon):
        self.line_no = -1
        self.scanner = re.Scanner(lexicon)

    def make_token(self, type, value):
        token = Token(type, value, self.line_no)
        self.line_no += value.count("\n")
        return token

    def lex(self, text):
        self.line_no = 0
        tokens, remainder = self.scanner.scan(text)
        if remainder:
            sys.stderr.write("Unlexable input:\n%s\n" % remainder)
            sys.exit(1)
        self.line_no = -1
        return tokens

class OuterRegionLexer(LineTrackingLexer):
    def __init__(self):
        super(OuterRegionLexer, self).__init__([
            (r'{{',                     self.lbrace),
            (r'}}',                     self.rbrace),
            (r'({(?!{)|}(?!})|[^{}])*', self.text)])
    def lbrace(self, scanner, token): return self.make_token(LBRACE, token)
    def rbrace(self, scanner, token): return self.make_token(RBRACE, token)
    def text(self, scanner, token):   return self.make_token(TEXT, token)

class OuterCommentLexer(OuterRegionLexer):
    def __init__(self):
        super(OuterRegionLexer, self).__init__([
            (r'/\*(.|[\r\n])*?\*/',                self.text),   # multiline comment
            (r'//(.|[\r\n])*?(?=[\r\n])',          self.text),   # single line comment
            (r'{{',                                self.lbrace),
            (r'}}',                                self.rbrace),
            (r'({(?!{)|}(?!})|/(?![/*])|[^{}/])*', self.text)])

class InnerLexer(OuterRegionLexer):
    def __init__(self):
        super(OuterRegionLexer, self).__init__([
            (r'{{',                               self.lbrace),
            (r'}}',                               self.rbrace),
            (r'(["\'])?((?:(?!\1)[^\\]|\\.)*)\1', self.quoted_id),
            (r'([^\s]+)',                         self.identifier),
            (r'\s+', None)])
    def identifier(self, scanner, token): return self.make_token(IDENTIFIER, token)
    def quoted_id(self, scanner, token):
        # remove quotes from quoted ids.  Note that ids and quoted ids are pretty much the same thing;
        # the quotes are just optional.  You only need them if you need spaces in your expression.
        return self.make_token(IDENTIFIER, re.sub(r'^["\'](.*)["\']$', '\\1', token))

# Global current filename and function name for error msgs
cur_filename = ""
cur_function = None

class WrapSyntaxError(Exception):
    """Simple Class for syntax errors raised by the wrapper generator (rather than python)"""
    pass

def syntax_error(msg):
    # TODO: make line numbers actually work.
    sys.stderr.write("%s:%d: %s\n" % (cur_filename, 0, msg))
    if cur_function:
        sys.stderr.write("    While handling %s.\n" % cur_function)
    raise WrapSyntaxError

################################################################################
# MPI Semantics:
#   Classes in this section describe MPI declarations and types.  These are used
#   to parse the mpi.h header and to generate wrapper code.
################################################################################
class Scope:
    """ This is the very basic class for scopes in the wrapper generator.  Scopes
        are hierarchical and support nesting.  They contain string keys mapped
        to either string values or to macro functions.
        Scopes also keep track of the particular macro they correspond to (macro_name).
    """
    def __init__(self, enclosing_scope=None):
        self.map = {}
        self.enclosing_scope = enclosing_scope
        self.macro_name = None           # For better debugging error messages

    def __getitem__(self, key):
        if key in self.map:         return self.map[key]
        elif self.enclosing_scope:  return self.enclosing_scope[key]
        else:                       raise KeyError(key + " is not in scope.")

    def __contains__(self, key):
        if key in self.map:         return True
        elif self.enclosing_scope:  return key in self.enclosing_scope
        else:                       return False

    def __setitem__(self, key, value):
        self.map[key] = value

    def include(self, map):
        """Add entire contents of the map (or scope) to this scope."""
        self.map.update(map)

################################################################################
# MPI Semantics:
#   Classes in this section describe MPI declarations and types.  These are used
#   to parse the mpi.h header and to generate wrapper code.
################################################################################
# Map from function name to declaration created from mpi.h.
mpi_functions = {}

class Param:
    """Descriptor for formal parameters of MPI functions.
       Doesn't represent a full parse, only the initial type information,
       name, and array info of the argument split up into strings.
    """
    def __init__(self, type, pointers, name, array, pos):
        self.type = type               # Name of arg's type (might include things like 'const')
        self.pointers = pointers       # Pointers
        self.name = name               # Formal parameter name (from header or autogenerated)
        self.array = array             # Any array type information after the name
        self.pos = pos                 # Position of arg in declartion
        self.decl = None               # This gets set later by Declaration

    def setDeclaration(self, decl):
        """Needs to be called by Declaration to finish initing the arg."""
        self.decl = decl

    def isHandleArray(self):
        """True if this Param represents an array of MPI handle values."""
        return (self.decl.name in mpi_array_calls
                and self.pos in mpi_array_calls[self.decl.name])

    def countParam(self):
        """If this Param is a handle array, returns the Param that represents the count of its elements"""
        return self.decl.args[mpi_array_calls[self.decl.name][self.pos]]

    def isHandle(self):
        """True if this Param is one of the MPI builtin handle types."""
        return self.type in mpi_handle_types

    def isStatus(self):
        """True if this Param is an MPI_Status.  MPI_Status is handled differently
           in c2f/f2c calls from the other handle types.
        """
        return self.type == "MPI_Status"

    def fortranFormal(self):
        """Prints out a formal parameter for a fortran wrapper."""
        # There are only a few possible fortran arg types in our wrappers, since
        # everything is a pointer.
        if self.type == "MPI_Aint" or self.type.endswith("_function"):
            ftype = self.type
        else:
            ftype = "MPI_Fint"

        # Arrays don't come in as pointers (they're passed as arrays)
        # Everything else is a pointer.
        if self.pointers:
            pointers = self.pointers
        elif self.array:
            pointers = ""
        else:
            pointers = "*"

        # Put it all together and return the fortran wrapper type here.
        arr = self.array or ''
        return "%s %s%s%s" % (ftype, pointers, self.name, arr)

    def cType(self):
        if not self.type:
            return ''
        else:
            arr = self.array or ''
            pointers = self.pointers or ''
            return "%s%s%s" % (self.type, pointers, arr)

    def cFormal(self):
        """Prints out a formal parameter for a C wrapper."""
        if not self.type:
            return self.name  # special case for '...'
        else:
            arr = self.array or ''
            pointers = self.pointers or ''
            return "%s %s%s%s" % (self.type, pointers, self.name, arr)

    def castType(self):
        arr = self.array or ''
        pointers = self.pointers or ''
        if '[]' in arr:
            if arr.count('[') > 1:
                pointers += '(*)'   # need extra parens for, e.g., int[][3] -> int(*)[3]
            else:
                pointers += '*'     # justa single array; can pass pointer.
            arr = arr.replace('[]', '')
        return "%s%s%s" % (self.type, pointers, arr)

    def __str__(self):
        return self.cFormal()


class Declaration:
    """ Descriptor for simple MPI function declarations.
        Contains return type, name of function, and a list of args.
    """
    def __init__(self, rtype, name):
        self.rtype = rtype
        self.name = name
        self.args = []

    def addArgument(self, arg):
        arg.setDeclaration(self)
        self.args.append(arg)

    def __iter__(self):
        for arg in self.args: yield arg

    def __str__(self):
        return self.prototype()

    def retType(self):
        return self.rtype

    def formals(self):
        return [arg.cFormal() for arg in self.args]

    def types(self):
        return [arg.cType() for arg in self.args]

    def argsNoEllipsis(self):
        return filter(lambda arg: arg.name != "...", self.args)

    def returnsErrorCode(self):
        """This is a special case for MPI_Wtime and MPI_Wtick.
           These functions actually return a double value instead of an int error code.
        """
        return self.rtype == "int"

    def argNames(self):
        return [arg.name for arg in self.argsNoEllipsis()]

    def getArgName(self, index):
        return self.argsNoEllipsis()[index].name

    def fortranFormals(self):
        formals = map(Param.fortranFormal, self.argsNoEllipsis())
        if self.name == "MPI_Init": formals = []    # Special case for init: no args in fortran

        ierr = []
        if self.returnsErrorCode(): ierr = ["MPI_Fint *ierr"]
        return formals + ierr

    def fortranArgNames(self):
        names = self.argNames()
        if self.name == "MPI_Init": names = []

        ierr = []
        if self.returnsErrorCode(): ierr = ["ierr"]
        return names + ierr

    def prototype(self, modifiers=""):
        if modifiers: modifiers = joinlines(modifiers, " ")
        return "%s%s %s(%s)" % (modifiers, self.retType(), self.name, ", ".join(self.formals()))

    def pmpi_prototype(self, modifiers=""):
        if modifiers: modifiers = joinlines(modifiers, " ")
        return "%s%s P%s(%s)" % (modifiers, self.retType(), self.name, ", ".join(self.formals()))

    def fortranPrototype(self, name=None, modifiers=""):
        if not name: name = self.name
        if modifiers: modifiers = joinlines(modifiers, " ")

        if self.returnsErrorCode():
            rtype = "void"  # Fortran calls use ierr parameter instead
        else:
            rtype = self.rtype
        return "%s%s %s(%s)" % (modifiers, rtype, name, ", ".join(self.fortranFormals()))


types = set()
all_pointers = set()

def enumerate_mpi_declarations(mpicc, includes):
    """ Invokes mpicc's C preprocessor on a C file that includes mpi.h.
        Parses the output for declarations, and yields each declaration to
        the caller.
    """
    # Create an input file that just includes <mpi.h>
    tmpfile = tempfile.NamedTemporaryFile('w+b', -1, suffix='.c')
    tmpname = "%s" % tmpfile.name
    tmpfile.write(b'#include <mpi.h>')
    tmpfile.write(b"\n")
    tmpfile.flush()

    # Run the mpicc -E on the temp file and pipe the output
    # back to this process for parsing.
    string_includes = ["-I"+dir for dir in includes]
    mpicc_cmd = "%s -E %s" % (mpicc, " ".join(string_includes))
    try:
        popen = subprocess.Popen("%s %s" % (mpicc_cmd, tmpname), shell=True,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except IOError:
        sys.stderr.write("IOError: couldn't run '" + mpicc_cmd + "' for parsing mpi.h\n")
        sys.exit(1)

    # Parse out the declarations from the MPI file
    mpi_h = popen.stdout
    for line in mpi_h:
        line = line.decode().strip()
        begin = begin_decl_re.search(line)
        if begin and not exclude_re.search(line):
            # Grab return type and fn name from initial parse
            return_type, fn_name = begin.groups()

            # Accumulate rest of declaration (possibly multi-line)
            while not end_decl_re.search(line):
                line += " " + next(mpi_h).decode().strip()

            # Split args up by commas so we can parse them independently
            fn_and_paren = r'(%s\s*\()' % fn_name
            match = re.search(fn_and_paren, line)
            lparen = match.start(1) + len(match.group(1)) - 1
            rparen = find_matching_paren(line, lparen)
            if rparen < 0:
                raise ValueError("Malformed declaration in header: '%s'" % line)

            arg_string = line[lparen+1:rparen]
            arg_list = list(map(lambda s: s.strip(), arg_string.split(",")))

            # Handle functions that take no args specially
            if arg_list == ['void']:
                arg_list = []

            # Parse formal parameter descriptors out of args
            decl = Declaration(return_type, fn_name)
            arg_num = 0
            for arg in arg_list:
                if arg == '...':   # Special case for Pcontrol.
                    decl.addArgument(Param(None, None, '...', None, arg_num))
                else:
                    match = formal_re.match(arg)
                    if not match:
                        sys.stderr.write("MATCH FAILED FOR: '%s' in %s\n" % (arg, fn_name))
                        sys.exit(1)

                    type, pointers, name, array = match.groups()
                    types.add(type)
                    all_pointers.add(pointers)
                    # If there's no name, make one up.
                    if not name: name = "arg_" + str(arg_num)

                    decl.addArgument(Param(type.strip(), pointers, name, array, arg_num))
                arg_num += 1

            yield decl

    mpi_h.close()
    return_code = popen.wait()
    if return_code != 0:
        sys.stderr.write("Error: Couldn't run '%s' for parsing mpi.h.\n" % mpicc_cmd)
        sys.stderr.write("       Process exited with code %d.\n" % return_code)
        sys.exit(1)

    # Do some cleanup once we're done reading.
    tmpfile.close()


def write_enter_guard(out, decl):
    """Prevent us from entering wrapper functions if we're already in a wrapper function.
       Just call the PMPI function w/o the wrapper instead."""
    if output_guards:
        out.write("    if (in_wrapper) return P%s(%s);\n" % (decl.name, ", ".join(decl.argNames())))
        out.write("    in_wrapper = 1;\n")

def write_exit_guard(out):
    """After a call, set in_wrapper back to 0 so we can enter the next call."""
    if output_guards:
        out.write("    in_wrapper = 0;\n")


def write_c_wrapper(out, decl, return_val, write_body):
    """Write the C wrapper for an MPI function."""
    # Write the PMPI prototype here in case mpi.h doesn't define it
    # (sadly the case with some MPI implementaitons)
    out.write(decl.pmpi_prototype(default_modifiers))
    out.write(";\n")

    # Now write the wrapper function, which will call the PMPI function we declared.
    out.write(decl.prototype(default_modifiers))
    out.write(" { \n")
    out.write("    %s %s = 0;\n" % (decl.retType(), return_val))

    write_enter_guard(out, decl)
    write_body(out)
    write_exit_guard(out)

    out.write("    return %s;\n" % return_val)
    out.write("}\n\n")


def write_fortran_binding(out, decl, delegate_name, binding, stmts=None):
    """Outputs a wrapper for a particular fortran binding that delegates to the
       primary Fortran wrapper.  Optionally takes a list of statements to execute
       before delegating.
    """
    out.write(decl.fortranPrototype(binding, default_modifiers))
    out.write(" { \n")
    if stmts:
        out.write(joinlines(map(lambda s: "    " + s, stmts)))
    if decl.returnsErrorCode():
        # regular MPI fortran functions use an error code
        out.write("    %s(%s);\n" % (delegate_name, ", ".join(decl.fortranArgNames())))
    else:
        # wtick and wtime return a value
        out.write("    return %s(%s);\n" % (delegate_name, ", ".join(decl.fortranArgNames())))
    out.write("}\n\n")


class FortranDelegation:
    """Class for constructing a call to a Fortran wrapper delegate function.  Provides
       storage for local temporary variables, copies of parameters, callsites for MPI-1 and
       MPI-2, and writebacks to local pointer types.
    """
    def __init__(self, decl, return_val):
        self.decl = decl
        self.return_val = return_val

        self.temps = set()
        self.copies = []
        self.writebacks = []
        self.actuals = []
        self.mpich_actuals = []

    def addTemp(self, type, name):
        """Adds a temp var with a particular name.  Adds the same var only once."""
        temp = "    %s %s;" % (type, name)
        self.temps.add(temp)

    def addActual(self, actual):
        self.actuals.append(actual)
        self.mpich_actuals.append(actual)

    def addActualMPICH(self, actual):
        self.mpich_actuals.append(actual)

    def addActualMPI2(self, actual):
        self.actuals.append(actual)

    def addWriteback(self, stmt):
        self.writebacks.append("    %s" % stmt)

    def addCopy(self, stmt):
        self.copies.append("    %s" % stmt)

    def write(self, out):
        assert len(self.actuals) == len(self.mpich_actuals)

        call = "    %s = %s" % (self.return_val, self.decl.name)
        mpich_call = "%s(%s);\n" % (call, ", ".join(self.mpich_actuals))
        mpi2_call = "%s(%s);\n" % (call, ", ".join(self.actuals))

        out.write("    %s %s = 0;\n" % (self.decl.retType(), self.return_val))
        if mpich_call == mpi2_call and not (self.temps or self.copies or self.writebacks):
            out.write(mpich_call)
        else:
            out.write("#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) && (MPICH_NAME == 1)) /* MPICH test */\n")
            out.write(mpich_call)
            out.write("#else /* MPI-2 safe call */\n")
            out.write(joinlines(self.temps))
            out.write(joinlines(self.copies))
            out.write(mpi2_call)
            out.write(joinlines(self.writebacks))
            out.write("#endif /* MPICH test */\n")


def write_fortran_wrappers(out, decl, return_val):
    """Writes primary fortran wrapper that handles arg translation.
       Also outputs bindings for this wrapper for different types of fortran compilers.
    """
    delegate_name = decl.name + f_wrap_suffix
    out.write(decl.fortranPrototype(delegate_name, ["static"]))
    out.write(" { \n")

    call = FortranDelegation(decl, return_val)

    if decl.name == "MPI_Init":
        # Use out.write() here so it comes at very beginning of wrapper function
        out.write("    int argc = 0;\n");
        out.write("    char ** argv = NULL;\n");
        call.addActual("&argc");
        call.addActual("&argv");
        call.write(out)
        out.write("    *ierr = %s;\n" % return_val)
        out.write("}\n\n")

        # Write out various bindings that delegate to the main fortran wrapper
        write_fortran_binding(out, decl, delegate_name, "MPI_INIT",   ["fortran_init = 1;"])
        write_fortran_binding(out, decl, delegate_name, "mpi_init",   ["fortran_init = 2;"])
        write_fortran_binding(out, decl, delegate_name, "mpi_init_",  ["fortran_init = 3;"])
        write_fortran_binding(out, decl, delegate_name, "mpi_init__", ["fortran_init = 4;"])
        return

    # This look processes the rest of the call for all other routines.
    for arg in decl.args:
        if arg.name == "...":   # skip ellipsis
            continue

        if not (arg.pointers or arg.array):
            if not arg.isHandle():
                # These are pass-by-value arguments, so just deref and pass thru
                dereferenced = "*%s" % arg.name
                call.addActual(dereferenced)
            else:
                # Non-ptr, non-arr handles need to be converted with MPI_Blah_f2c
                # No special case for MPI_Status here because MPI_Statuses are never passed by value.
                call.addActualMPI2("%s_f2c(*%s)" % (conversion_prefix(arg.type), arg.name))
                call.addActualMPICH("(%s)(*%s)" % (arg.type, arg.name))

        else:
            if not arg.isHandle():
                # Non-MPI handle pointer types can be passed w/o dereferencing, but need to
                # cast to correct pointer type first (from MPI_Fint*).
                call.addActual("(%s)%s" % (arg.castType(), arg.name))
            else:
                # For MPI-1, assume ints, cross fingers, and pass things straight through.
                call.addActualMPICH("(%s*)%s" % (arg.type, arg.name))
                conv = conversion_prefix(arg.type)
                temp = "temp_%s" % arg.name

                # For MPI-2, other pointer and array types need temporaries and special conversions.
                if not arg.isHandleArray():
                    call.addTemp(arg.type, temp)
                    call.addActualMPI2("&%s" % temp)

                    if arg.isStatus():
                        call.addCopy("%s_f2c(%s, &%s);"  % (conv, arg.name, temp))
                        call.addWriteback("%s_c2f(&%s, %s);" % (conv, temp, arg.name))
                    else:
                        call.addCopy("%s = %s_f2c(*%s);"  % (temp, conv, arg.name))
                        call.addWriteback("*%s = %s_c2f(%s);" % (arg.name, conv, temp))
                else:
                    # Make temporary variables for the array and the loop var
                    temp_arr_type = "%s*" % arg.type
                    call.addTemp(temp_arr_type, temp)
                    call.addTemp("int", "i")

                    # generate a copy and a writeback statement for this type of handle
                    if arg.isStatus():
                        copy = "    %s_f2c(&%s[i], &%s[i])"  % (conv, arg.name, temp)
                        writeback = "    %s_c2f(&%s[i], &%s[i])" % (conv, temp, arg.name)
                    else:
                        copy = "    temp_%s[i] = %s_f2c(%s[i])"  % (arg.name, conv, arg.name)
                        writeback = "    %s[i] = %s_c2f(temp_%s[i])" % (arg.name, conv, arg.name)

                    # Generate the call surrounded by temp array allocation, copies, writebacks, and temp free
                    count = "*%s" % arg.countParam().name
                    call.addCopy("%s = (%s)malloc(sizeof(%s) * %s);" %
                                 (temp, temp_arr_type, arg.type, count))
                    call.addCopy("for (i=0; i < %s; i++)" % count)
                    call.addCopy("%s;" % copy)
                    call.addActualMPI2(temp)
                    call.addWriteback("for (i=0; i < %s; i++)" % count)
                    call.addWriteback("%s;" % writeback)
                    call.addWriteback("free(%s);" % temp)

    call.write(out)
    if decl.returnsErrorCode():
        out.write("    *ierr = %s;\n" % return_val)
    else:
        out.write("    return %s;\n" % return_val)
    out.write("}\n\n")

    # Write out various bindings that delegate to the main fortran wrapper
    write_fortran_binding(out, decl, delegate_name, decl.name.upper())
    write_fortran_binding(out, decl, delegate_name, decl.name.lower())
    write_fortran_binding(out, decl, delegate_name, decl.name.lower() + "_")
    write_fortran_binding(out, decl, delegate_name, decl.name.lower() + "__")


################################################################################
# Macros:
#   - functions annotated as @macro or @bodymacro define the global macros and
#     basic pieces of the generator.
#   - include_decl is used to include MPI declarations into function scopes.
################################################################################
# Table of global macros
macros = {}

# This decorator adds macro functions to the outermost function scope.
def macro(macro_name, **attrs):
    def decorate(fun):
        macros[macro_name] = fun # Add macro to outer scope under supplied name
        fun.has_body = False     # By default, macros have no body.
        for key in attrs:        # Optionally set/override attributes
            setattr(fun, key, attrs[key])
        return fun
    return decorate

def handle_list(list_name, list, args):
    """This function handles indexing lists used as macros in the wrapper generator.
       There are two syntaxes:
       {{<list_name>}}          Evaluates to the whole list, e.g. 'foo, bar, baz'
       {{<list_name> <index>}}  Evaluates to a particular element of a list.
    """
    if not args:
        return list
    else:
        len(args) == 1 or syntax_error("Wrong number of args for list expression.")
        try:
            return list[int(args[0])]
        except ValueError:
            syntax_error("Invald index value: '%s'" % args[0])
        except IndexError:
            syntax_error("Index out of range in '%s': %d" % (list_name, index))

class TypeApplier:
    """This class implements a Macro function for applying something callable to
       args in a decl with a particular type.
    """
    def __init__(self, decl):
        self.decl = decl

    def __call__(self, out, scope, args, children):
        len(args) == 2 or syntax_error("Wrong number of args in apply macro.")
        type, macro_name = args
        for arg in self.decl.args:
            if arg.cType() == type:
                out.write("%s(%s);\n" % (macro_name, arg.name))

def include_decl(scope, decl):
    """This function is used by macros to include attributes MPI declarations in their scope."""
    scope["ret_type"] = decl.retType()
    scope["args"]     = decl.argNames()
    scope["nargs"]    = len(decl.argNames())
    scope["types"]    = decl.types()
    scope["formals"]  = decl.formals()
    scope["apply_to_type"] = TypeApplier(decl)
    scope.function_name  = decl.name

    # These are old-stype, deprecated names.
    def get_arg(out, scope, args, children):
        return handle_list("args", decl.argNames(), args)
    scope["get_arg"]     = get_arg
    scope["applyToType"] = scope["apply_to_type"]
    scope["retType"]     = scope["ret_type"]
    scope["argList"]     = "(%s)" % ", ".join(scope["args"])
    scope["argTypeList"] = "(%s)" % ", ".join(scope["formals"])

def all_but(fn_list):
    """Return a list of all mpi functions except those in fn_list"""
    all_mpi = set(mpi_functions.keys())
    diff = all_mpi - set(fn_list)
    return [x for x in sorted(diff)]

@macro("foreachfn", has_body=True)
def foreachfn(out, scope, args, children):
    """Iterate over all functions listed in args."""
    args or syntax_error("Error: foreachfn requires function name argument.")
    global cur_function

    fn_var = args[0]
    for fn_name in args[1:]:
        cur_function = fn_name
        if not fn_name in mpi_functions:
            syntax_error(fn_name + " is not an MPI function")

        fn = mpi_functions[fn_name]
        fn_scope = Scope(scope)
        fn_scope[fn_var] = fn_name
        include_decl(fn_scope, fn)

        for child in children:
            child.evaluate(out, fn_scope)
    cur_function = None

@macro("fn", has_body=True)
def fn(out, scope, args, children):
    """Iterate over listed functions and generate skeleton too."""
    args or syntax_error("Error: fn requires function name argument.")
    global cur_function

    fn_var = args[0]
    for fn_name in args[1:]:
        cur_function = fn_name
        if not fn_name in mpi_functions:
            syntax_error(fn_name + " is not an MPI function")

        fn = mpi_functions[fn_name]
        return_val = "_wrap_py_return_val"

        fn_scope = Scope(scope)
        fn_scope[fn_var] = fn_name
        include_decl(fn_scope, fn)

        fn_scope["ret_val"] = return_val
        fn_scope["returnVal"]  = fn_scope["ret_val"]  # deprecated name.

        c_call = "%s = P%s(%s);" % (return_val, fn.name, ", ".join(fn.argNames()))
        if fn_name == "MPI_Init" and output_fortran_wrappers:
            def callfn(out, scope, args, children):
                # All this is to deal with fortran, since fortran's MPI_Init() function is different
                # from C's.  We need to make sure to delegate specifically to the fortran init wrapping.
                # For dynamic libs, we use weak symbols to pick it automatically.  For static libs, need
                # to rely on input from the user via pmpi_init_binding and the -i option.
                out.write("    if (fortran_init) {\n")
                out.write("#ifdef PIC\n")
                out.write("        if (!PMPI_INIT && !pmpi_init && !pmpi_init_ && !pmpi_init__) {\n")
                out.write("            fprintf(stderr, \"ERROR: Couldn't find fortran pmpi_init function.  Link against static library instead.\\n\");\n")
                out.write("            exit(1);\n")
                out.write("        }")
                out.write("        switch (fortran_init) {\n")
                out.write("        case 1: PMPI_INIT(&%s);   break;\n" % return_val)
                out.write("        case 2: pmpi_init(&%s);   break;\n" % return_val)
                out.write("        case 3: pmpi_init_(&%s);  break;\n" % return_val)
                out.write("        case 4: pmpi_init__(&%s); break;\n" % return_val)
                out.write("        default:\n")
                out.write("            fprintf(stderr, \"NO SUITABLE FORTRAN MPI_INIT BINDING\\n\");\n")
                out.write("            break;\n")
                out.write("        }\n")
                out.write("#else /* !PIC */\n")
                out.write("        %s(&%s);\n" % (pmpi_init_binding, return_val))
                out.write("#endif /* !PIC */\n")
                out.write("    } else {\n")
                out.write("        %s\n" % c_call)
                out.write("    }\n")

            fn_scope["callfn"] = callfn

            def write_fortran_init_flag():
                output.write("static int fortran_init = 0;\n")
            once(write_fortran_init_flag)

        else:
            fn_scope["callfn"] = c_call

        def write_body(out):
            for child in children:
                child.evaluate(out, fn_scope)

        out.write("/* ================== C Wrappers for %s ================== */\n" % fn_name)
        write_c_wrapper(out, fn, return_val, write_body)
        if output_fortran_wrappers:
            out.write("/* =============== Fortran Wrappers for %s =============== */\n" % fn_name)
            write_fortran_wrappers(out, fn, return_val)
            out.write("/* ================= End Wrappers for %s ================= */\n\n\n" % fn_name)
    cur_function = None

@macro("forallfn", has_body=True)
def forallfn(out, scope, args, children):
    """Iterate over all but the functions listed in args."""
    args or syntax_error("Error: forallfn requires function name argument.")
    foreachfn(out, scope, [args[0]] + all_but(args[1:]), children)

@macro("fnall", has_body=True)
def fnall(out, scope, args, children):
    """Iterate over all but listed functions and generate skeleton too."""
    args or syntax_error("Error: fnall requires function name argument.")
    fn(out, scope, [args[0]] + all_but(args[1:]), children)

@macro("sub")
def sub(out, scope, args, children):
    """{{sub <string> <regexp> <substitution>}}
       Replaces value of <string> with all instances of <regexp> replaced with <substitution>.
    """
    len(args) == 3 or syntax_error("'sub' macro takes exactly 4 arguments.")
    string, regex, substitution = args
    if isinstance(string, list):
        return [re.sub(regex, substitution, s) for s in string]
    if not isinstance(regex, str):
        syntax_error("Invalid regular expression in 'sub' macro: '%s'" % regex)
    else:
        return re.sub(regex, substitution, string)

@macro("zip")
def zip_macro(out, scope, args, children):
    len(args) == 2 or syntax_error("'zip' macro takes exactly 2 arguments.")
    if not all([isinstance(a, list) for a in args]):
        syntax_error("Arguments to 'zip' macro must be lists.")
    a, b = args
    return ["%s %s" % x for x in zip(a, b)]

@macro("def")
def def_macro(out, scope, args, children):
    len(args) == 2 or syntax_error("'def' macro takes exactly 2 arguments.")
    scope[args[0]] = args[1]

@macro("list")
def list_macro(out, scope, args, children):
    result = []
    for arg in args:
        if isinstance(arg, list):
            result.extend(arg)
        else:
            result.append(arg)
    return result

@macro("filter")
def filter_macro(out, scope, args, children):
    """{{filter <regex> <list>}}
       Returns a list containing all elements of <list> that <regex> matches.
    """
    len(args) == 2 or syntax_error("'filter' macro takes exactly 2 arguments.")
    regex, l = args
    if not isinstance(l, list):
        syntax_error("Invalid list in 'filter' macro: '%s'" % str(list))
    if not isinstance(regex, str):
        syntax_error("Invalid regex in 'filter' macro: '%s'" % str(regex))
    def match(s):
        return re.search(regex, s)
    return list(filter(match, l))

@macro("fn_num")
def fn_num(out, scope, args, children):
    val = fn_num.val
    fn_num.val += 1
    return val
fn_num.val = 0  # init the counter here.


################################################################################
# Parser support:
#   - Chunk class for bits of parsed text on which macros are executed.
#   - parse() function uses a Lexer to examine a file.
################################################################################
class Chunk:
    """Represents a piece of a wrapper file.  Is either a text chunk
       or a macro chunk with children to which the macro should be applied.
       macros are evaluated lazily, so the macro is just a string until
       execute is called and it is fetched from its enclosing scope."""
    def __init__(self):
        self.macro    = None
        self.args     = []
        self.text     = None
        self.children = []

    def iwrite(self, file, level, text):
        """Write indented text."""
        for x in xrange(level):
            file.write("  ")
        file.write(text)

    def write(self, file=sys.stdout, l=0):
        if self.macro: self.iwrite(file, l, "{{%s %s}}" % (self.macro, " ".join([str(arg) for arg in self.args])))
        if self.text:  self.iwrite(file, l, "TEXT\n")
        for child in self.children:
            child.write(file, l+1)

    def execute(self, out, scope):
        """This function executes a chunk.  For strings, lists, text chunks, etc., this just
           entails returning the chunk's value.  For callable macros, this executes and returns
           the chunk's value.
        """
        if not self.macro:
            out.write(self.text)
        else:
            if not self.macro in scope:
                error_msg = "Invalid macro: '%s'" % self.macro
                if scope.function_name:
                    error_msg += " for " + scope.function_name
                syntax_error(error_msg)

            value = scope[self.macro]
            if hasattr(value, "__call__"):
                # It's a macro, so we need to execute it.  But first evaluate its args.
                def eval_arg(arg):
                    if isinstance(arg, Chunk):
                        return arg.execute(out, scope)
                    else:
                        return arg
                args = [eval_arg(arg) for arg in self.args]
                return value(out, scope, args, self.children)
            elif isinstance(value, list):
                # Special case for handling lists and list indexing
                return handle_list(self.macro, value, self.args)
            else:
                # Just return the value of anything else
                return value

    def stringify(self, value):
        """Used by evaluate() to print the return values of chunks out to the output file."""
        if isinstance(value, list):
            return ", ".join(value)
        else:
            return str(value)

    def evaluate(self, out, scope):
        """This is an 'interactive' version of execute.  This should be called when
           the chunk's value (if any) should be written out.  Body macros and the outermost
           scope should use this instead of execute().
        """
        value = self.execute(out, scope)
        if value is not None:  # Note the distinction here -- 0 is false but we want to print it!
            out.write(self.stringify(value))

class Parser:
    """Parser for the really simple wrappergen grammar.
       This parser has support for multiple lexers.  self.tokens is a list of iterables, each
       representing a new token stream.  You can add additional tokens to be lexed using push_tokens.
       This will cause the pushed tokens to be handled before any others.  This allows us to switch
       lexers while parsing, so that the outer part of the file is processed in a language-agnostic
       way, but stuff inside macros is handled as its own macro language.
    """
    def __init__(self, macros):
        self.macros = macros
        self.macro_lexer = InnerLexer()
        self.tokens = iter([]) # iterators over tokens, handled in order.  Starts empty.
        self.token = None      # last accepted token
        self.next = None       # next token

    def gettok(self):
        """Puts the next token in the input stream into self.next."""
        try:
            self.next = next(self.tokens)
        except StopIteration:
            self.next = None

    def push_tokens(self, iterable):
        """Adds all tokens in some iterable to the token stream."""
        self.tokens = itertools.chain(iter(iterable), iter([self.next]), self.tokens)
        self.gettok()

    def accept(self, id):
        """Puts the next symbol in self.token if we like it.  Then calls gettok()"""
        if self.next.isa(id):
            self.token = self.next
            self.gettok()
            return True
        return False

    def unexpected_token(self):
        syntax_error("Unexpected token: %s." % self.next)

    def expect(self, id):
        """Like accept(), but fails if we don't like the next token."""
        if self.accept(id):
            return True
        else:
            if self.next:
                self.unexpected_token()
            else:
                syntax_error("Unexpected end of file.")
            sys.exit(1)

    def is_body_macro(self, name):
        """Shorthand for testing whether a particular name is the name of a macro that has a body.
           Need this for parsing the language b/c things like {{fn}} need a corresponding {{endfn}}.
        """
        return name in self.macros and self.macros[name].has_body

    def macro(self, accept_body_macros=True):
        # lex inner-macro text as wrapper language if we encounter text here.
        if self.accept(TEXT):
            self.push_tokens(self.macro_lexer.lex(self.token.value))

        # Now proceed with parsing the macro language's tokens
        chunk = Chunk()
        self.expect(IDENTIFIER)
        chunk.macro = self.token.value

        if not accept_body_macros and self.is_body_macro(chunk.macro):
            syntax_error("Cannot use body macros in expression context: '%s'" % chunk.macro)
            eys.exit(1)

        while True:
            if self.accept(LBRACE):
                chunk.args.append(self.macro(False))
            elif self.accept(IDENTIFIER):
                chunk.args.append(self.token.value)
            elif self.accept(TEXT):
                self.push_tokens(self.macro_lexer.lex(self.token.value))
            else:
                self.expect(RBRACE)
                break
        return chunk

    def text(self, end_macro = None):
        chunks = []
        while self.next:
            if self.accept(TEXT):
                chunk = Chunk()
                chunk.text = self.token.value
                chunks.append(chunk)
            elif self.accept(LBRACE):
                chunk = self.macro()
                name = chunk.macro

                if name == end_macro:
                    # end macro: just break and don't append
                    break
                elif isindex(chunk.macro):
                    # Special case for indices -- raw number macros index 'args' list
                    chunk.macro = "args"
                    chunk.args = [name]
                elif self.is_body_macro(name):
                    chunk.children = self.text("end"+name)
                chunks.append(chunk)
            else:
                self.unexpected_token()

        return chunks

    def parse(self, text):
        if skip_headers:
            outer_lexer = OuterRegionLexer()   # Not generating C code, text is text.
        else:
            outer_lexer = OuterCommentLexer()  # C code. Considers C-style comments.
        self.push_tokens(outer_lexer.lex(text))
        return self.text()

################################################################################
# Main script:
#   Get arguments, set up outer scope, parse files, generator wrappers.
################################################################################
def usage():
    sys.stderr.write(usage_string)
    sys.exit(2)

# Let the user specify another mpicc to get mpi.h from
output = sys.stdout
output_filename = None

try:
    opts, args = getopt.gnu_getopt(sys.argv[1:], "fsgdc:o:i:I:")
except getopt.GetoptError as err:
    sys.stderr.write(err + "\n")
    usage()

for opt, arg in opts:
    if opt == "-d": dump_prototypes = True
    if opt == "-f": output_fortran_wrappers = True
    if opt == "-s": skip_headers = True
    if opt == "-g": output_guards = True
    if opt == "-c": mpicc = arg
    if opt == "-o": output_filename = arg
    if opt == "-I":
        stripped = arg.strip()
        if stripped: includes.append(stripped)
    if opt == "-i":
        if not arg in pmpi_init_bindings:
            sys.stderr.write("ERROR: PMPI_Init binding must be one of:\n    %s\n" % " ".join(possible_bindings))
            usage()
        else:
            pmpi_init_binding = arg

if len(args) < 1 and not dump_prototypes:
    usage()

# Parse mpi.h and put declarations into a map.
for decl in enumerate_mpi_declarations(mpicc, includes):
    mpi_functions[decl.name] = decl
    if dump_prototypes: print(decl)

# Fail gracefully if we didn't find anything.
if not mpi_functions:
    sys.stderr.write("Error: Found no declarations in mpi.h.\n")
    sys.exit(1)

# If we're just dumping prototypes, we can just exit here.
if dump_prototypes: sys.exit(0)

# Open the output file here if it was specified
if output_filename:
    try:
        output = open(output_filename, "w")
    except IOError:
        sys.stderr.write("Error: couldn't open file " + arg + " for writing.\n")
        sys.exit(1)

try:
    # Start with some headers and definitions.
    if not skip_headers:
        output.write(wrapper_includes)
        if output_guards: output.write("static int in_wrapper = 0;\n")

    # Parse each file listed on the command line and execute
    # it once it's parsed.
    fileno = 0
    for f in args:
        cur_filename = f
        file = open(cur_filename)

        # Outer scope contains fileno and the fundamental macros.
        outer_scope = Scope()
        outer_scope["fileno"] = str(fileno)
        outer_scope.include(macros)

        parser = Parser(macros)
        chunks = parser.parse(file.read())

        for chunk in chunks:
            chunk.evaluate(output, Scope(outer_scope))
        fileno += 1

except WrapSyntaxError:
    output.close()
    if output_filename: os.remove(output_filename)
    sys.exit(1)

output.close()
