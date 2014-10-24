/// @file Variant.cpp
/// Variant datatype implementation

#include "Variant.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iterator>
#include <map>
#include <sstream>

using namespace cali;
using namespace std;


Variant::Variant(ctx_attr_type type, std::size_t size, const void* data)
    : m_type { type }, m_size { size }
{
    switch (m_type) {
    case CTX_TYPE_INV:
        break;
    case CTX_TYPE_USR:
    case CTX_TYPE_STRING:
        m_value.ptr = data;
        break;
    case CTX_TYPE_INT:
        m_value.v_int = *static_cast<const int64_t*>(data);
        break;
    case CTX_TYPE_ADDR:
    case CTX_TYPE_UINT:
        m_value.v_uint = *static_cast<const uint64_t*>(data);
        break;
    case CTX_TYPE_DOUBLE:
        m_value.v_uint = *static_cast<const double*>(data);
        break;
    case CTX_TYPE_BOOL:
        m_value.v_bool = *static_cast<const bool*>(data);
        break;
    case CTX_TYPE_TYPE:
        m_value.v_type = *static_cast<const ctx_attr_type*>(data);
        break;
    }
}

ctx_id_t
Variant::to_id(bool* okptr)  
{
    return to_uint(okptr);
}

bool
Variant::to_bool(bool* okptr)  
{
    bool ok = false;

    if (m_type == CTX_TYPE_INV && !m_string.empty()) {
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
            m_type = CTX_TYPE_BOOL;
            m_size = sizeof(bool);
        }
    }

    ok = (m_type == CTX_TYPE_BOOL || m_type == CTX_TYPE_INT || m_type == CTX_TYPE_UINT);

    if (okptr)
        *okptr = ok;

    switch (m_type) {
    case CTX_TYPE_BOOL:
        return m_value.v_bool;
    case CTX_TYPE_INT:
        return m_value.v_int  != 0;
    case CTX_TYPE_UINT:
        return m_value.v_uint != 0;
    default:
        break;
    }

    return false;
}

int
Variant::to_int(bool* okptr)  
{
    if (m_type == CTX_TYPE_INV && !m_string.empty()) {
        istringstream is(m_string);

        is >> m_value.v_int;

        if (is) {
            m_type = CTX_TYPE_INT;
            m_size = sizeof(int64_t);
        }
    }

    bool ok = (m_type == CTX_TYPE_INT);

    if (okptr)
        *okptr = ok;

    return ok ? m_value.v_int : 0;
}

unsigned
Variant::to_uint(bool* okptr) 
{
    if (m_type == CTX_TYPE_INV && !m_string.empty()) {
        istringstream is(m_string);

        is >> m_value.v_uint;

        if (is) {
            m_type = CTX_TYPE_UINT;
            m_size = sizeof(uint64_t);
        }
    }

    bool ok = (m_type == CTX_TYPE_UINT);

    if (okptr)
        *okptr = ok;

    return ok ? m_value.v_uint : 0;
}

double
Variant::to_double(bool* okptr)  
{
    if (m_type == CTX_TYPE_INV && !m_string.empty()) {
        istringstream is(m_string);

        is >> m_value.v_double;

        if (is) {
            m_type = CTX_TYPE_DOUBLE;
            m_size = sizeof(double);
        }
    }

    bool ok = (m_type == CTX_TYPE_DOUBLE || m_type == CTX_TYPE_INT || m_type == CTX_TYPE_UINT);

    if (okptr)
        *okptr = ok;

    return ok ? m_value.v_double : 0;
}

ctx_attr_type
Variant::to_attr_type(bool* okptr) 
{
    if (m_type == CTX_TYPE_INV && !m_string.empty()) {
        const map<string, ctx_attr_type> typemap = { 
            { "usr",    CTX_TYPE_USR    },
            { "int",    CTX_TYPE_INT    }, 
            { "uint",   CTX_TYPE_UINT   },
            { "string", CTX_TYPE_STRING },
            { "addr",   CTX_TYPE_ADDR   },
            { "double", CTX_TYPE_DOUBLE },
            { "bool",   CTX_TYPE_BOOL   },
            { "type",   CTX_TYPE_TYPE   } };

        auto it = typemap.find(m_string);

        if (it != typemap.end()) {
            m_value.v_type = it->second;
            m_type = CTX_TYPE_TYPE;
        }   
    }

    bool ok = (m_type == CTX_TYPE_TYPE);

    if (okptr)
        *okptr = ok;

    return ok ? m_value.v_type : CTX_TYPE_INV;
}

std::string
Variant::to_string() const
{
    if (!m_string.empty())
        return m_string;

    string ret;

    switch (m_type) {
    case CTX_TYPE_INV:
        break;
    case CTX_TYPE_USR:
    {
        ostringstream os;

        copy(static_cast<const unsigned char*>(m_value.ptr), static_cast<const unsigned char*>(m_value.ptr) + m_size,
             ostream_iterator<unsigned>(os << std::hex << setw(2) << setfill('0'), ":"));

        ret = os.str();
    }
        break;
    case CTX_TYPE_INT:
        ret = std::to_string(m_value.v_int);
        break;
    case CTX_TYPE_UINT:
        ret = std::to_string(m_value.v_uint);
        break;
    case CTX_TYPE_STRING:
        ret.assign(static_cast<const char*>(m_value.ptr), m_size);
        break;
    case CTX_TYPE_ADDR:
    {
        ostringstream os;
        os << std::hex << m_value.v_uint;
        ret = os.str();
    }
        break;
    case CTX_TYPE_DOUBLE:
        ret = std::to_string(m_value.v_double);
        break;
    case CTX_TYPE_BOOL:
        ret = m_value.v_bool ? "true" : "false";
        break;
    case CTX_TYPE_TYPE:
    {
        const char* tstr[] = { "usr", "int", "uint", "string", "addr", "double", "bool", "type" };
        ret = (m_value.v_type < 0 || m_value.v_type > CTX_TYPE_TYPE) ? "invalid" : tstr[m_value.v_type];
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

ostream& cali::operator << (ostream& os, const Variant& v)
{
    os << v.to_string();
    return os;
}
