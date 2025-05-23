// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file Variant.h
/// A variant datatype

#pragma once

#include "cali_variant.h"

#include <iostream>
#include <string>
#include <type_traits>

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

    constexpr Variant() : m_v { CALI_TYPE_INV, { static_cast<uint64_t>(0) } } {}

    Variant(const cali_variant_t& v) : m_v(v) {}

    Variant(bool val) : m_v(cali_make_variant_from_bool(val)) {}
    Variant(int val) : m_v(cali_make_variant_from_int(val)) {}
    Variant(double val) : m_v(cali_make_variant_from_double(val)) {}
    Variant(const char* val) : m_v(cali_make_variant_from_string(val)) {}
    Variant(cali_attr_type val) : m_v(cali_make_variant_from_type(val)) {}

    template <typename U, typename std::enable_if<std::is_unsigned<U>::value, int>::type = 0>
    Variant(U val) : m_v(cali_make_variant_from_uint(val))
    {}

    Variant(cali_attr_type type, const void* data, std::size_t size) { m_v = cali_make_variant(type, data, size); }

    bool empty() const { return cali_variant_is_empty(m_v); };

    operator bool () const { return !empty(); }

    bool has_unmanaged_data() const
    {
        cali_attr_type t = type();
        return (t == CALI_TYPE_STRING || t == CALI_TYPE_USR);
    }

    cali_variant_t c_variant() const { return m_v; }
    cali_attr_type type() const { return cali_variant_get_type(m_v); }
    const void* data() const { return cali_variant_get_data(&m_v); }
    size_t size() const { return cali_variant_get_size(m_v); }
    void* get_ptr() const { return cali_variant_get_ptr(m_v); }

    cali_id_t to_id(bool* okptr = nullptr) const;
    int to_int(bool* okptr = nullptr) const { return cali_variant_to_int(m_v, okptr); }
    int64_t to_int64(bool* okptr = nullptr) const { return cali_variant_to_int64(m_v, okptr); }
    uint64_t to_uint(bool* okptr = nullptr) const { return cali_variant_to_uint(m_v, okptr); }
    bool to_bool(bool* okptr = nullptr) const { return cali_variant_to_bool(m_v, okptr); }
    double to_double(bool* okptr = nullptr) const { return cali_variant_to_double(m_v, okptr); }
    cali_attr_type to_attr_type(bool* okptr = nullptr) const { return cali_variant_to_type(m_v, okptr); }

    std::string to_string() const;

    Variant copy(void* ptr) const
    {
        Variant to(*this);

        if (has_unmanaged_data())
            to.m_v.value.unmanaged_ptr = memcpy(ptr, m_v.value.unmanaged_const_ptr, size());

        return to;
    }

    Variant& operator+= (const Variant& val);

    Variant& min(const Variant& val);
    Variant& max(const Variant& val);
    Variant  div(unsigned count);

    static void update_minmaxsum(const Variant& val, Variant& min_val, Variant& max_val, Variant& sum_val);

    size_t pack(unsigned char* buf) const { return cali_variant_pack(m_v, buf); }

    std::ostream& write_cali(std::ostream& os);

    static Variant unpack(const unsigned char* buf, size_t* inc, bool* ok = nullptr)
    {
        return {cali_variant_unpack(buf, inc, ok)};
    }

    static Variant from_string(cali_attr_type type, const char* str);

    // vector<unsigned char> data() const;

    friend bool operator== (const Variant& lhs, const Variant& rhs);
    friend bool operator!= (const Variant& lhs, const Variant& rhs);
    friend bool operator< (const Variant& lhs, const Variant& rhs);
    friend bool operator> (const Variant& lhs, const Variant& rhs);
};

inline bool operator== (const Variant& lhs, const Variant& rhs)
{
    return cali_variant_eq(lhs.m_v, rhs.m_v);
}

inline bool operator!= (const Variant& lhs, const Variant& rhs)
{
    return !cali_variant_eq(lhs.m_v, rhs.m_v);
}

inline bool operator< (const Variant& lhs, const Variant& rhs)
{
    return (cali_variant_compare(lhs.m_v, rhs.m_v) < 0);
}

inline bool operator> (const Variant& lhs, const Variant& rhs)
{
    return (cali_variant_compare(lhs.m_v, rhs.m_v) > 0);
}

std::ostream& operator<< (std::ostream& os, const Variant& v);

} // namespace cali
