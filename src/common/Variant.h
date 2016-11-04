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

/// @file Variant.h
/// A variant datatype

#ifndef CALI_VARIANT_H
#define CALI_VARIANT_H

#include "cali_types.h"

#include "util/shared_obj.hpp"

#include <cstring>
#include <string>
#include <iostream>

namespace cali
{

class Variant 
{
    cali_attr_type m_type;
    std::size_t    m_size;

    util::shared_obj<std::string> m_string;

    // need some sort of "managed/copied" and "unmanaged" pointers
    union Value {
        bool           v_bool;
        double         v_double;
        int64_t        v_int;
        uint64_t       v_uint;
        cali_attr_type v_type;
        const void*    ptr;
    }              m_value;

public:

    Variant() 
        : m_type { CALI_TYPE_INV }, m_size { 0 }
        { }

    Variant(const Variant& v) = default;
    Variant(Variant&& v) = default;

    explicit Variant(const std::string& string)
        : m_type { CALI_TYPE_INV }, m_size { 0 }, m_string { string }, m_value { 0 }
        { }

    Variant(bool val)
        : m_type { CALI_TYPE_BOOL   }, m_size { sizeof(bool) }
        { m_value.v_bool = val; }
    Variant(int val)
        : m_type { CALI_TYPE_INT    }, m_size { sizeof(int64_t) }
        { m_value.v_int  = val; } 
    Variant(double val)
        : m_type { CALI_TYPE_DOUBLE }, m_size { sizeof(double) }
        { m_value.v_double = val; }
    Variant(unsigned val)
        : m_type { CALI_TYPE_UINT   }, m_size { sizeof(uint64_t) }
        { m_value.v_uint = val; }
    Variant(cali_id_t val)
        : m_type { CALI_TYPE_UINT   }, m_size { sizeof(uint64_t) }
        { m_value.v_uint = val; }
    Variant(cali_attr_type val)
        : m_type { CALI_TYPE_TYPE   }, m_size { sizeof(cali_attr_type) }
        { m_value.v_type = val; }

    Variant(cali_attr_type type, const void* data, std::size_t size);

    ~Variant()
        { }

    Variant& operator = (const Variant& v) = default;

    bool empty() const  { 
        return (m_type == CALI_TYPE_INV || m_size == 0) && m_string.empty(); 
    };
    operator bool() const {
        return !empty();
    }

    cali_attr_type type() const { return m_type; }
    const void*    data() const;
    size_t         size() const { return m_size; }

    cali_id_t      to_id(bool* okptr = nullptr);
    cali_id_t      to_id(bool* okptr = nullptr) const;
    int            to_int(bool* okptr = nullptr);
    int            to_int(bool* okptr = nullptr) const;
    uint64_t       to_uint(bool* okptr = nullptr);
    uint64_t       to_uint(bool* okptr = nullptr) const;
    bool           to_bool(bool* okptr = nullptr);
    bool           to_bool(bool* okptr = nullptr) const;
    double         to_double(bool* okptr = nullptr);
    double         to_double(bool* okptr = nullptr) const;
    cali_attr_type to_attr_type(bool* okptr = nullptr);
    cali_attr_type to_attr_type(bool* okptr = nullptr) const;

    std::string    to_string() const;

    size_t         pack(unsigned char* buf) const;
    static Variant unpack(const unsigned char* buf, size_t* inc, bool* ok);

    Variant        concretize(cali_attr_type type, bool* okptr) const;
    
    // vector<unsigned char> data() const;

    friend bool operator == (const Variant& lhs, const Variant& rhs);
    friend bool operator <  (const Variant& lhs, const Variant& rhs);
};

bool operator == (const Variant& lhs, const Variant& rhs);
bool operator <  (const Variant& lhs, const Variant& rhs);

std::ostream& operator << (std::ostream& os, const Variant& v);

} // namespace cali

#endif
