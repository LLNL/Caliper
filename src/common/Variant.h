/// @file Variant.h
/// A variant datatype

#ifndef CALI_VARIANT_H
#define CALI_VARIANT_H

#include "cali_types.h"

#include <cstring>
#include <string>
#include <iostream>

namespace cali
{

class Variant 
{
    cali_attr_type m_type;
    std::size_t    m_size;

    std::string    m_string;

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
    unsigned       to_uint(bool* okptr = nullptr);
    unsigned       to_uint(bool* okptr = nullptr) const;
    bool           to_bool(bool* okptr = nullptr);
    double         to_double(bool* okptr = nullptr);
    double         to_double(bool* okptr = nullptr) const;
    cali_attr_type to_attr_type(bool* okptr = nullptr);

    std::string    to_string();
    std::string    to_string() const;

    // vector<unsigned char> data() const;

    friend bool operator == (const Variant& lhs, const Variant& rhs);
};

bool operator == (const Variant& lhs, const Variant& rhs);

std::ostream& operator << (std::ostream& os, const Variant& v);

} // namespace cali

#endif
