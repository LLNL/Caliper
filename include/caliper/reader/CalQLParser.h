// Copyright (c) 2017, Lawrence Livermore National Security, LLC.  
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

/// \file CalQLParser.h
/// \brief CalQLParser definition

#pragma once

#include "QuerySpec.h"

#include <iostream>
#include <memory>
#include <string>

namespace cali
{

/// \brief Create a QuerySpec specification from a given %CalQL expression.
/// \ingroup ReaderAPI

class CalQLParser
{
    struct CalQLParserImpl;
    std::unique_ptr<CalQLParserImpl> mP;
    
public:

    /// \brief Read %CalQL expression from the given input stream \a is.
    CalQLParser(std::istream& is);
    /// \brief Read %CalQL expression from \a str.
    CalQLParser(const char* str);
    
    ~CalQLParser();

    /// \brief Indicate if there was an error parsing the %CalQL expression.
    bool
    error() const;

    /// \brief Approximate position of a parser error in the given string or stream.
    std::istream::pos_type
    error_pos();

    /// \brief A descriptive error message in case of a parse error.
    std::string
    error_msg();

    /// \brief Returns the query specification object for the given %CalQL expression.
    QuerySpec
    spec() const;
}; 

}
