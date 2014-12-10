/// @file Variant.cpp
/// Variant datatype implementation

#include "Variant.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <map>
#include <sstream>

using namespace cali;
using namespace std;

Variant::Variant(cali_attr_type type, const void* data, std::size_t size)
    : m_type { type }, m_size { size }
{
    switch (m_type) {
    case CALI_TYPE_INV:
        break;
    case CALI_TYPE_USR:
    case CALI_TYPE_STRING:
        m_value.ptr = data;
        break;
    case CALI_TYPE_INT:
        m_value.v_int    = *static_cast<const int64_t*>(data);
        break;
    case CALI_TYPE_ADDR:
    case CALI_TYPE_UINT:
        m_value.v_uint   = *static_cast<const uint64_t*>(data);
        break;
    case CALI_TYPE_DOUBLE:
        m_value.v_double = *static_cast<const double*>(data);
        break;
    case CALI_TYPE_BOOL:
        m_value.v_bool   = *static_cast<const bool*>(data);
        break;
    case CALI_TYPE_TYPE:
        m_value.v_type   = *static_cast<const cali_attr_type*>(data);
        break;
    }
}

const void*
Variant::data() const
{
    switch (m_type) {
    case CALI_TYPE_USR:
    case CALI_TYPE_STRING:
        return m_value.ptr;
        break;
    case CALI_TYPE_INT:
        return &m_value.v_int;
        break;
    case CALI_TYPE_ADDR:
    case CALI_TYPE_UINT:
        return &m_value.v_uint;
        break;
    case CALI_TYPE_DOUBLE:
        return &m_value.v_double;
        break;
    case CALI_TYPE_BOOL:
        return &m_value.v_bool;
        break;
    case CALI_TYPE_TYPE:
        return &m_value.v_type;
        break;        
    default:
        break;
    }

    return nullptr;
}

cali_id_t
Variant::to_id(bool* okptr)  
{
    return to_uint(okptr);
}

bool
Variant::to_bool(bool* okptr)  
{
    bool ok = false;

    if (m_type == CALI_TYPE_INV && !m_string.empty()) {
        // try string
        {
            string lower;

            std::transform(m_string.begin(), m_string.end(), back_inserter(lower), ::tolower);

            if (lower == "true" || lower == "t") {
                ok = true;
                m_value.v_bool = true;
            } else if (lower == "false" || lower == "f") {
                ok = true;
                m_value.v_bool = false;
            }
        }

        // try numeral
        if (!ok) {
            istringstream is(m_string);

            is >> m_value.v_bool;
            ok = !is.fail();
        }

        if (ok) {
            m_type = CALI_TYPE_BOOL;
            m_size = sizeof(bool);
        }
    }

    ok = (m_type == CALI_TYPE_BOOL || m_type == CALI_TYPE_INT || m_type == CALI_TYPE_UINT);

    if (okptr)
        *okptr = ok;

    switch (m_type) {
    case CALI_TYPE_BOOL:
        return m_value.v_bool;
    case CALI_TYPE_INT:
        return m_value.v_int  != 0;
    case CALI_TYPE_UINT:
        return m_value.v_uint != 0;
    default:
        break;
    }

    return false;
}

int
Variant::to_int(bool* okptr)  
{
    if (m_type == CALI_TYPE_INV && !m_string.empty()) {
        istringstream is(m_string);

        is >> m_value.v_int;

        if (is) {
            m_type = CALI_TYPE_INT;
            m_size = sizeof(int64_t);
        }
    }

    bool ok = (m_type == CALI_TYPE_INT);

    if (okptr)
        *okptr = ok;

    return ok ? m_value.v_int : 0;
}

unsigned
Variant::to_uint(bool* okptr) 
{
    if (m_type == CALI_TYPE_INV && !m_string.empty()) {
        istringstream is(m_string);

        is >> m_value.v_uint;

        if (is) {
            m_type = CALI_TYPE_UINT;
            m_size = sizeof(uint64_t);
        }
    }

    bool ok = (m_type == CALI_TYPE_UINT);

    if (okptr)
        *okptr = ok;

    return ok ? m_value.v_uint : 0;
}

double
Variant::to_double(bool* okptr)  
{
    if (m_type == CALI_TYPE_INV && !m_string.empty()) {
        istringstream is(m_string);

        is >> m_value.v_double;

        if (is) {
            m_type = CALI_TYPE_DOUBLE;
            m_size = sizeof(double);
        }
    }

    bool ok = (m_type == CALI_TYPE_DOUBLE || m_type == CALI_TYPE_INT || m_type == CALI_TYPE_UINT);

    if (okptr)
        *okptr = ok;

    switch (m_type) {
    case CALI_TYPE_DOUBLE:
        return m_value.v_double;
    case CALI_TYPE_INT:
        return m_value.v_int;
    case CALI_TYPE_UINT:
        return m_value.v_uint;
    default:
        return 0;
    }
}

cali_attr_type
Variant::to_attr_type(bool* okptr) 
{
    if (m_type == CALI_TYPE_INV && !m_string.empty()) {
        m_value.v_type = cali_string2type(m_string.c_str());

        if (m_value.v_type != CALI_TYPE_INV)
            m_type = CALI_TYPE_TYPE;
    }

    bool ok = (m_type == CALI_TYPE_TYPE);

    if (okptr)
        *okptr = ok;

    return ok ? m_value.v_type : CALI_TYPE_INV;
}

std::string
Variant::to_string() const
{
    if (!m_string.empty())
        return m_string;

    string ret;

    switch (m_type) {
    case CALI_TYPE_INV:
        break;
    case CALI_TYPE_USR:
    {
        ostringstream os;

        copy(static_cast<const unsigned char*>(m_value.ptr), static_cast<const unsigned char*>(m_value.ptr) + m_size,
             ostream_iterator<unsigned>(os << std::hex << setw(2) << setfill('0'), ":"));

        ret = os.str();
    }
        break;
    case CALI_TYPE_INT:
        ret = std::to_string(m_value.v_int);
        break;
    case CALI_TYPE_UINT:
        ret = std::to_string(m_value.v_uint);
        break;
    case CALI_TYPE_STRING:
        ret.assign(static_cast<const char*>(m_value.ptr), m_size);
        break;
    case CALI_TYPE_ADDR:
    {
        ostringstream os;
        os << std::hex << m_value.v_uint;
        ret = os.str();
    }
        break;
    case CALI_TYPE_DOUBLE:
        ret = std::to_string(m_value.v_double);
        break;
    case CALI_TYPE_BOOL:
        ret = m_value.v_bool ? "true" : "false";
        break;
    case CALI_TYPE_TYPE:
    {
        ret = cali_type2string(m_value.v_type);
    }
        break;
    }

    return ret;
}

std::string
Variant::to_string() 
{
    m_string = const_cast<const Variant*>(this)->to_string();
    return m_string;
}

bool cali::operator == (const Variant& lhs, const Variant& rhs)
{
    if (lhs.m_type == CALI_TYPE_INV || rhs.m_type == CALI_TYPE_INV)
        return lhs.to_string() == rhs.to_string();

    if (lhs.m_type != rhs.m_type)
        return false;

    switch (lhs.m_type) {
    case CALI_TYPE_STRING:
    case CALI_TYPE_USR:
        if (lhs.m_size == rhs.m_size) {
            if (lhs.m_value.ptr == rhs.m_value.ptr)
                return true;
            else
                return 0 == memcmp(lhs.m_value.ptr, rhs.m_value.ptr, lhs.m_size);
        }
        return false;
    default:
        return lhs.m_value.v_uint == rhs.m_value.v_uint;
    }
}

ostream& cali::operator << (ostream& os, const Variant& v)
{
    os << v.to_string();
    return os;
}
