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

/// \file StringConverter.cpp
/// StringConverter implementation

#include "caliper/common/StringConverter.h"

#include "util/parse_util.h"

#include <algorithm>
#include <cctype>
#include <sstream>

cali_id_t
cali::StringConverter::to_id() const
{
    bool ok = false;
    cali_id_t id = to_uint(&ok);

    return ok ? id : CALI_INV_ID;
}

bool
cali::StringConverter::to_bool(bool* okptr) const
{
    bool ok  = false;
    bool res = false;
    
    // try string
    
    {
        std::string lower;

        std::transform(m_str.begin(), m_str.end(), std::back_inserter(lower),
                       ::tolower);

        if (lower == "true" ||
            lower == "t"    ||
            lower == "yes") {
            ok  = true;
            res = true;
        } else if (lower == "false" ||
                   lower == "f"     ||
                   lower == "no") {
            ok  = true;
            res = false;
        }
    }

    // try numeral
    
    if (!ok) {
        try {
            unsigned long i = std::stoul(m_str);
            
            res = (i != 0);
            ok  = true;
        } catch (...) {
            ok  = false;
        }
    }

    if (okptr)
        *okptr = ok;

    return res;
}

int
cali::StringConverter::to_int(bool* okptr) const
{
    bool ok  = false;
    int  res = 0;

    try {
        res = std::stoi(m_str);
        ok  = true;
    } catch (...) {
        ok  = false;
    }

    if (okptr)
        *okptr = ok;

    return res;
}

uint64_t
cali::StringConverter::to_uint(bool* okptr, int base) const
{
    bool ok = false;
    uint64_t res = 0;

    try {
        res = std::stoull(m_str, nullptr, base);
        ok  = true;
    } catch (...) {
        ok  = false;
    }

    if (okptr)
        *okptr = ok;

    return res;    
}

double
cali::StringConverter::to_double(bool* okptr) const
{
    bool   ok  = false;
    double res = 0;

    try {
        res = std::stod(m_str);
        ok  = true;
    } catch (...) {
        ok  = false;
    }

    if (okptr)
        *okptr = ok;

    return res;
}

std::vector<std::string>
cali::StringConverter::to_stringlist(const char* separators, bool* okptr) const
{
    std::vector<std::string> ret;
    char c;

    std::istringstream is(m_str);

    do { 
        std::string str = util::read_word(is, separators);

        if (!str.empty())
            ret.push_back(str);

        c = util::read_char(is);
    } while (is.good() && util::is_one_of(c, separators));

    if (okptr)
        *okptr = true;

    return ret;
}
