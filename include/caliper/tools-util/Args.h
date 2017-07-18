// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/// \file Args.h
/// \brief Parse command line arguments.

#ifndef UTIL_ARGS_H
#define UTIL_ARGS_H

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace util
{

/// \brief Parse command-line arguments.

class Args {
    struct ArgsImpl;
    std::shared_ptr<ArgsImpl> mP;

public:

    struct Table {
        const char* name;

        const char* longopt;
        char        shortopt;

        bool        has_argument;

        const char* info;
        const char* argument_info;
      
        static const Table Terminator;
    };

    Args();

    Args(const Table table[]);

    ~Args();

    bool set_fail(bool fail = true);
    void add_table(const Table table[]);

    // --- Parse

    /// Parses command-line options given by \a argc and \a argv.
    /// \return \a argv[] index of first unknown option argument
    int  parse(int argc, const char* const argv[], int pos = 1);

    // --- Retrieval

    std::string program_name() const;

    std::string get(const std::string& name, const std::string& def = "") const;
    bool        is_set(const std::string& name) const;

    std::vector<std::string> options() const;
    std::vector<std::string> arguments() const;

    // --- Info 

    void print_available_options(std::ostream& os) const;
};

}

#endif
