// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file Variant.cpp
/// Variant datatype implementation

#include "caliper/common/Variant.h"

#include "caliper/common/StringConverter.h"

#include "util/format_util.h"
#include "util/parse_util.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <sstream>

using namespace cali;

cali_id_t Variant::to_id(bool* okptr) const
{
    bool      ok = false;
    cali_id_t id = to_uint(&ok);

    if (okptr)
        *okptr = ok;

    return ok ? id : CALI_INV_ID;
}

std::string Variant::to_string() const
{
    std::string ret;

    switch (this->type()) {
    case CALI_TYPE_INV:
        break;
    case CALI_TYPE_USR:
        {
            size_t      size = this->size();
            const void* ptr  = data();

            std::ostringstream os;

            std::copy(
                static_cast<const unsigned char*>(ptr),
                static_cast<const unsigned char*>(ptr) + size,
                std::ostream_iterator<unsigned>(os << std::hex << std::setw(2) << std::setfill('0'), ":")
            );

            ret = os.str();
        }
        break;
    case CALI_TYPE_INT:
        ret = std::to_string(m_v.value.v_int);
        break;
    case CALI_TYPE_UINT:
        ret = std::to_string(m_v.value.v_uint);
        break;
    case CALI_TYPE_STRING:
        {
            const char* str = static_cast<const char*>(data());
            std::size_t len = size();

            if (len && str[len - 1] == 0)
                --len;

            ret.assign(str, len);
        }
        break;
    case CALI_TYPE_ADDR:
        {
            std::ostringstream os;
            os << std::hex << to_uint();
            ret = os.str();
        }
        break;
    case CALI_TYPE_DOUBLE:
        ret = std::to_string(m_v.value.v_double);
        break;
    case CALI_TYPE_BOOL:
        ret = to_bool() ? "true" : "false";
        break;
    case CALI_TYPE_TYPE:
        ret = cali_type2string(to_attr_type());
        break;
    case CALI_TYPE_PTR:
        {
            std::ostringstream os;
            os << std::hex << reinterpret_cast<uint64_t>(data());
            ret = os.str();
        }
    }

    return ret;
}

Variant Variant::from_string(cali_attr_type type, const char* str)
{
    switch (type) {
    case CALI_TYPE_INV:
    case CALI_TYPE_USR:
    case CALI_TYPE_PTR:
        return Variant();
    case CALI_TYPE_STRING:
        return Variant(CALI_TYPE_STRING, str, strlen(str));
    case CALI_TYPE_INT:
        {
            char*     str_end = nullptr;
            long long ll      = std::strtoll(str, &str_end, 10);
            return str != str_end ? Variant(cali_make_variant_from_int64(ll)) : Variant();
        }
    case CALI_TYPE_ADDR:
        {
            bool     ok = false;
            uint64_t u  = StringConverter(str).to_uint(&ok, 16);
            return ok ? Variant(CALI_TYPE_ADDR, &u, sizeof(uint64_t)) : Variant();
        }
    case CALI_TYPE_UINT:
        {
            auto p = util::str_to_uint64(str);
            return p.first ? Variant(cali_make_variant_from_uint(p.second)) : Variant();
        }
    case CALI_TYPE_DOUBLE:
        {
            char*  str_end = nullptr;
            double d       = std::strtod(str, &str_end);
            return str != str_end ? Variant(d) : Variant();
        }
    case CALI_TYPE_BOOL:
        {
            bool ok = false;
            bool b  = StringConverter(str).to_bool(&ok);
            return ok ? Variant(b) : Variant();
        }
    case CALI_TYPE_TYPE:
        {
            cali_attr_type type = cali_string2type(str);
            return (type != CALI_TYPE_INV) ? Variant(type) : Variant();
        }
    }

    return Variant();
}

std::ostream& Variant::write_cali(std::ostream& os)
{
    cali_attr_type type = this->type();

    switch (type) {
    case CALI_TYPE_INV:
        break;
    case CALI_TYPE_INT:
        os << m_v.value.v_int;
        break;
    case CALI_TYPE_DOUBLE:
        os << m_v.value.v_double;
        break;
    case CALI_TYPE_UINT:
        util::write_uint64(os, m_v.value.v_uint);
        break;
    case CALI_TYPE_STRING:
        util::write_cali_esc_string(os, static_cast<const char*>(m_v.value.unmanaged_const_ptr), size());
        break;
    case CALI_TYPE_TYPE:
        os << cali_type2string(m_v.value.v_type);
        break;
    default:
        util::write_cali_esc_string(os, to_string());
    }

    return os;
}

std::ostream& cali::operator<< (std::ostream& os, const Variant& v)
{
    os << v.to_string();
    return os;
}

Variant& Variant::operator+= (const Variant& val)
{
    cali_attr_type type = this->type();

    if (type == val.type()) {
        switch (type) {
        case CALI_TYPE_DOUBLE:
            m_v.value.v_double += val.m_v.value.v_double;
            break;
        case CALI_TYPE_INT:
            m_v.value.v_int += val.m_v.value.v_int;
            break;
        case CALI_TYPE_UINT:
            m_v.value.v_uint += val.m_v.value.v_uint;
            break;
        default:
            break;
        }
    } else {
        switch (type) {
        case CALI_TYPE_INV:
            *this = val;
            break;
        case CALI_TYPE_DOUBLE:
            m_v.value.v_double += val.to_double();
            break;
        case CALI_TYPE_INT:
            m_v.value.v_int += val.to_int64();
            break;
        case CALI_TYPE_UINT:
            m_v.value.v_uint += val.to_uint();
            break;
        default:
            break;
        }
    }

    return *this;
}

Variant& Variant::min(const Variant& val)
{
    cali_attr_type type = this->type();

    if (type == val.type()) {
        switch (type) {
        case CALI_TYPE_DOUBLE:
            m_v.value.v_double = std::min(m_v.value.v_double, val.m_v.value.v_double);
            break;
        case CALI_TYPE_INT:
            m_v.value.v_int = std::min(m_v.value.v_int, val.m_v.value.v_int);
            break;
        case CALI_TYPE_UINT:
            m_v.value.v_uint = std::min(m_v.value.v_uint, val.m_v.value.v_uint);
            break;
        default:
            break;
        }
    } else {
        switch (type) {
        case CALI_TYPE_INV:
            *this = val;
            break;
        case CALI_TYPE_DOUBLE:
            m_v.value.v_double = std::min(m_v.value.v_double, val.to_double());
            break;
        case CALI_TYPE_INT:
            m_v.value.v_int = std::min(m_v.value.v_int, val.to_int64());
            break;
        case CALI_TYPE_UINT:
            m_v.value.v_uint = std::min(m_v.value.v_uint, val.to_uint());
            break;
        default:
            break;
        }
    }

    return *this;
}

Variant& Variant::max(const Variant& val)
{
    cali_attr_type type = this->type();

    if (type == val.type()) {
        switch (type) {
        case CALI_TYPE_DOUBLE:
            m_v.value.v_double = std::max(m_v.value.v_double, val.m_v.value.v_double);
            break;
        case CALI_TYPE_INT:
            m_v.value.v_int = std::max(m_v.value.v_int, val.m_v.value.v_int);
            break;
        case CALI_TYPE_UINT:
            m_v.value.v_uint = std::max(m_v.value.v_uint, val.m_v.value.v_uint);
            break;
        default:
            break;
        }
    } else {
        switch (type) {
        case CALI_TYPE_INV:
            *this = val;
            break;
        case CALI_TYPE_DOUBLE:
            m_v.value.v_double = std::max(m_v.value.v_double, val.to_double());
            break;
        case CALI_TYPE_INT:
            m_v.value.v_int = std::max(m_v.value.v_int, val.to_int64());
            break;
        case CALI_TYPE_UINT:
            m_v.value.v_uint = std::max(m_v.value.v_uint, val.to_uint());
            break;
        default:
            break;
        }
    }

    return *this;
}

Variant Variant::div(unsigned count)
{
    cali_attr_type type = this->type();

    switch (type) {
    case CALI_TYPE_DOUBLE:
        return Variant(m_v.value.v_double / static_cast<double>(count));
    case CALI_TYPE_INT:
        return Variant(cali_make_variant_from_int64(m_v.value.v_int / static_cast<int64_t>(count)));
    case CALI_TYPE_UINT:
        return Variant(cali_make_variant_from_uint(m_v.value.v_uint / count));
    default:
        break;
    }

    return Variant();
}

void Variant::update_minmaxsum(const Variant& val, Variant& min_val, Variant& max_val, Variant& sum_val)
{
    if (min_val.empty()) {
        min_val = val;
        max_val = val;
        sum_val = val;
        return;
    }

    switch (val.m_v.type_and_size & CALI_VARIANT_TYPE_MASK) {
    case CALI_TYPE_DOUBLE:
        {
            double d = val.m_v.value.v_double;
            sum_val.m_v.value.v_double += d;
            if (d < min_val.m_v.value.v_double)
                min_val.m_v.value.v_double = d;
            else if (d > max_val.m_v.value.v_double)
                max_val.m_v.value.v_double = d;
        }
        break;
    case CALI_TYPE_INT:
        {
            int64_t i = val.m_v.value.v_int;
            sum_val.m_v.value.v_int += i;
            if (i < min_val.m_v.value.v_int)
                min_val.m_v.value.v_int = i;
            else if (i > max_val.m_v.value.v_int)
                max_val.m_v.value.v_int = i;
        }
        break;
    case CALI_TYPE_UINT:
        {
            uint64_t u = val.m_v.value.v_uint;
            sum_val.m_v.value.v_uint += u;
            if (u < min_val.m_v.value.v_uint)
                min_val.m_v.value.v_uint = u;
            else if (u > max_val.m_v.value.v_uint)
                max_val.m_v.value.v_uint = u;
        }
        break;
    default:
        break;
    }
}
