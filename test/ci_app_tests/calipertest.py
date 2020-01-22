##############################################################################
# Copyright (c) 2017, Lawrence Livermore National Security, LLC.
# Produced at the Lawrence Livermore National Laboratory.
# 
# This file is part of Caliper.
# Created by David Boehme, boehme3@llnl.gov, All rights reserved.
# LLNL-CODE-678900
# 
# For details, see https://github.com/scalability-llnl/Caliper.
# Please also see the LICENSE file for our additional BSD notice.
# 
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
# 
# * Redistributions of source code must retain the above copyright notice, this list of
#   conditions and the disclaimer below.
# * Redistributions in binary form must reproduce the above copyright notice, this list of
#   conditions and the disclaimer (as noted below) in the documentation and/or other materials
#   provided with the distribution.
# * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
#   or promote products derived from this software without specific prior written permission.
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
##############################################################################

#
# Utilities and helper functions for the Caliper application/workflow test framework 
#

import os
import subprocess

class CaliperExecError(Exception):
    """ Error class when running a process fails
    """

    def __init__(self, msg):
        self.msg = msg

    def __str__(self):
        return repr(self.msg)
    
    
def run_test_with_query(target_cmd, query_cmd, env):
    """ Execute a command and query, and return query command's output
    
        This will run the command given by target_cmd with the environment given 
        by env, and pipe its stdout output into the command given by query_cmd.
    """

    target_proc = subprocess.Popen(target_cmd, env=env, stdout=subprocess.PIPE)
    query_proc  = subprocess.Popen(query_cmd, stdin=target_proc.stdout, stdout=subprocess.PIPE)
    target_proc.stdout.close()
    query_out, query_err = query_proc.communicate()
    target_proc.wait()

    if (target_proc.returncode != 0):
        raise CaliperExecError('Command ' + str(target_cmd) + ' exited with ' + str(target_proc.returncode))
    if (query_proc.returncode != 0):
        raise CaliperExecError('Command ' + str(query_cmd) + ' exited with ' + str(query_proc.returncode))
    
    return query_out


def run_test(target_cmd, env):
    """ Execute a command, and return its output """

    target_proc = subprocess.Popen(target_cmd, env=env, stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    proc_out, proc_err = target_proc.communicate()

    if (target_proc.returncode != 0):
        raise CaliperExecError('Command ' + str(target_cmd) + ' exited with ' + str(target_proc.returncode))

    return (proc_out,proc_err)


def get_snapshots_from_text(query_output):
    """ Translate expanded snapshots from `cali-query -e` output into list of dicts 

        Takes input in the form 

          attr1=x,attr2=y
          attr1=z
          ...

        and converts it to `[ { 'attr1' : 'x', 'attr2' : 'y' }, { 'attr1' : 'z' } ]`
    """

    snapshots = []

    for line in query_output.decode().splitlines():
        snapshots.append( { kv.partition('=')[0] : kv.partition('=')[2] for kv in line.split(',') } )

    return snapshots


def has_snapshot_with_attributes(snapshots, attrs):
    """ Test if a snapshot with the given subset of attributes exists """

    def dict_contains(a, b):
        for k in b.keys():
            if (a.get(k) != b.get(k)):
                return False
        return True

    for s in snapshots:
        if (dict_contains(s, attrs)):
            return True

    return False
    
def has_snapshot_with_keys(snapshots, keys):
    """ Test if a snapshot with the given subset of keys exists """
    return any( set(keys).issubset(set(s.keys())) for s in snapshots )

def get_snapshot_with_keys(snapshots, keys):
    """ Return the first snapshot that with the given subset of keys"""

    sk = set(keys)

    for s in snapshots:
        if (sk.issubset(set(s.keys()))):
            return s

    return None
