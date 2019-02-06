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

/// \file StringConverter.h
/// A class to convert strings into various other data types.
/// This is primarily a convenience class to transparently replace
/// cali::Variant's former string conversion capabilities.
 
#pragma once

#include "cali_types.h"

#include <string>
#include <vector>

namespace cali
{

class StringConverter {
    std::string m_str;

public:

    StringConverter()
    : m_str()
    { }

    StringConverter(const std::string& str)
        : m_str(str)
    { }

    cali_id_t     to_id() const;
    
    bool          to_bool(bool* okptr = nullptr) const;

    int           to_int(bool* okptr = nullptr)  const;
    uint64_t      to_uint(bool* okptr = nullptr, int base = 10) const;

    double        to_double(bool* okptr = nullptr) const;

    std::string   to_string(bool* okptr = nullptr) const {
        if (okptr)
            *okptr = true;
        return m_str;
    }

    std::vector<std::string>
    to_stringlist(const char* separators = ",", bool* okptr = nullptr) const;
};
    
} // namespace cali
