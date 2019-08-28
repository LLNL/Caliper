// Copyright (c) 2015-2017, Lawrence Livermore National Security, LLC.  
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

/// \file Variant.h
/// A variant datatype

#pragma once

#include "cali_types.h"
#include "cali_variant.h"
#include "caliper/caliper-config.h"
#include <string>
#include <iostream>

namespace cali
{

    /// \brief Encapsulate values of various Caliper data types
    ///
    /// This class encapsulates data values in Caliper, and implements
    /// most of the data type-specific functionality. It is a thin wrapper
    /// around the `cali_variant_t` C data type.
    ///
    /// NOTE: This class does *not* do any sort of memory management:
    /// strings and "blobs" are stored as unmanaged pointers. Users need
    /// to make sure these pointers are valid while any variant
    /// encapsulating them is being used.
    
class Variant 
{
    cali_variant_t m_v;
    
public:
    
    CONSTEXPR_UNLESS_PGI Variant()
        : m_v { CALI_TYPE_INV, { .v_uint = 0 } }  { }
    
    Variant(const cali_variant_t& v)
        : m_v(v) { }
    
    Variant(bool val)
        : m_v(cali_make_variant_from_bool(val))   { }
    Variant(int val)
        : m_v(cali_make_variant_from_int(val))    { }
    Variant(double val)
        : m_v(cali_make_variant_from_double(val)) { }
    //Variant(unsigned long val) : m_v(cali_make_variant_from_uint(val)) {}

    struct internal_true_type{};
    struct internal_false_type{};
    template<typename V, typename U>
    struct is_same {
        using type = internal_false_type;
    };
    template<typename T>
    struct is_same<T,T> {
        using type = internal_true_type;
    };
    template<typename V, typename U>
    struct is_different {
        using type = internal_true_type;
    };
    template<typename T>
    struct is_different<T,T> {
        using type = internal_false_type;
    };

    template<typename B, class T=void>
    struct enable_if{};

    template<class T>
    struct enable_if<internal_true_type,T> { using type = T;};

    //template<typename T=unsigned long,typename sfinae=typename enable_if<typename is_different<T,uint64_t>::type,void>::type>
    //Variant(uint64_t val)
    //        : m_v(cali_make_variant_from_uint(val))   { }

    template<typename T, typename sfinae=typename enable_if<typename is_same<cali_variant_t,decltype(cali_make_variant_from_uint(T()))>::type,void>::type>
    Variant(T val) : m_v(cali_make_variant_from_uint(val)) {}

    Variant(const char* val)
            : m_v(cali_make_variant_from_string(val))   { }
    Variant(cali_attr_type val)
        : m_v(cali_make_variant_from_type(val))   { }

    Variant(cali_attr_type type, const void* data, std::size_t size)
    {
        m_v = cali_make_variant(type, data, size);
    }

    bool empty() const  { 
        return (m_v.type_and_size & CALI_VARIANT_TYPE_MASK) == CALI_TYPE_INV;
    };
    operator bool() const {
        return !empty();
    }

    cali_variant_t c_variant() const { return m_v; }
    
    cali_attr_type type() const    { return cali_variant_get_type(m_v);  }
    const void*    data() const    { return cali_variant_get_data(&m_v); }
    size_t         size() const    { return cali_variant_get_size(m_v);  }

    void*          get_ptr() const { return cali_variant_get_ptr(m_v);   }
    
    cali_id_t      to_id(bool* okptr = nullptr) const;
    int            to_int(bool* okptr = nullptr) const {
        return cali_variant_to_int(m_v, okptr);
    }
    uint64_t       to_uint(bool* okptr = nullptr) const {
        return cali_variant_to_uint(m_v, okptr);
    }
    bool           to_bool(bool* okptr = nullptr) const {
        return cali_variant_to_bool(m_v, okptr);
    }
    double         to_double(bool* okptr = nullptr) const {
        return cali_variant_to_double(m_v, okptr);
    }
    cali_attr_type to_attr_type(bool* okptr = nullptr) const {
        return cali_variant_to_type(m_v, okptr);
    }
    
    std::string    to_string() const;

    size_t         pack(unsigned char* buf) const {
        return cali_variant_pack(m_v, buf);
    }
    
    static Variant unpack(const unsigned char* buf, size_t* inc, bool* ok) {
        return Variant(cali_variant_unpack(buf, inc, ok));
    }

    static Variant from_string(cali_attr_type type, const char* str, bool* ok = nullptr);
    
    // vector<unsigned char> data() const;

    friend bool operator == (const Variant& lhs, const Variant& rhs);
    friend bool operator <  (const Variant& lhs, const Variant& rhs);
    friend bool operator >  (const Variant& lhs, const Variant& rhs);
};

inline bool operator == (const Variant& lhs, const Variant& rhs) {
    return cali_variant_eq(lhs.m_v, rhs.m_v);
}
    
inline bool operator <  (const Variant& lhs, const Variant& rhs) {
    return (cali_variant_compare(lhs.m_v, rhs.m_v) < 0);
}

inline bool operator >  (const Variant& lhs, const Variant& rhs) {
    return (cali_variant_compare(lhs.m_v, rhs.m_v) > 0);
}

std::ostream& operator << (std::ostream& os, const Variant& v);

} // namespace cali

