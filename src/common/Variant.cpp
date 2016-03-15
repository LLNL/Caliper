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

/// @file Variant.cpp
/// Variant datatype implementation

#include "Variant.h"

#include "c-util/vlenc.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <map>
#include <sstream>
#include <vector>

using namespace cali;
using namespace std;

namespace
{
    struct Histogram {
        const int        nbins;
        std::vector<int> bins;

        void update(int i) {
            int bin = 0;

            for (bin = 0; bin < nbins-1 && i > 2*bin; ++bin)
                ;

            ++bins[bin];
        }

        void print() const {
            int bound = 0;
    
            for (int val : bins)
                std::cerr << 2*bound++ << ": " << val << ",  ";
        }
    };
}

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
Variant::to_id(bool* okptr) const
{
    bool      ok = false;
    cali_id_t id = to_uint(&ok);

    if (okptr)
        *okptr = ok;

    return ok ? id : CALI_INV_ID;
}

cali_id_t
Variant::to_id(bool* okptr)
{
    bool      ok = false;
    cali_id_t id = to_uint(&ok);

    if (okptr)
        *okptr = ok;

    return ok ? id : CALI_INV_ID;
}

bool
Variant::to_bool(bool* okptr)  
{
    bool ok = false;

    if (m_type == CALI_TYPE_INV && !m_string.empty()) {
        // try string
        {
            string lower;

            std::transform(m_string.obj().begin(), m_string.obj().end(), back_inserter(lower), ::tolower);

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
            istringstream is(m_string.obj());

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
Variant::to_int(bool* okptr) const
{
    int               i = static_cast<int>(m_value.v_int);
    cali_attr_type type = m_type;

    if (m_type == CALI_TYPE_INV && !m_string.empty()) {
        istringstream is(m_string.obj());

        is >> i;

        if (is)
            type = CALI_TYPE_INT;
    }

    bool ok = (type == CALI_TYPE_INT);

    if (okptr)
        *okptr = ok;

    return ok ? i : 0;
}

int
Variant::to_int(bool* okptr) 
{
    bool ok = false;
    int   i = const_cast<const Variant*>(this)->to_int(&ok);

    if (m_type == CALI_TYPE_INV && ok) {
        m_type        = CALI_TYPE_INT;
        m_size        = sizeof(int64_t);
        m_value.v_int = i;
    }

    if (okptr)
        *okptr = ok;

    return i;
}

unsigned
Variant::to_uint(bool* okptr) const
{
    unsigned       uint = static_cast<unsigned>(m_value.v_uint);
    cali_attr_type type = m_type;

    if (m_type == CALI_TYPE_INV && !m_string.empty()) {
        istringstream is(m_string.obj());

        is >> uint;

        if (is)
            type = CALI_TYPE_UINT;
    }

    bool ok = (type == CALI_TYPE_UINT);

    if (okptr)
        *okptr = ok;

    return ok ? uint : 0;
}

unsigned
Variant::to_uint(bool* okptr) 
{
    bool       ok = false;
    unsigned uint = const_cast<const Variant*>(this)->to_uint(&ok);

    if (m_type == CALI_TYPE_INV && ok) {
        m_type         = CALI_TYPE_UINT;
        m_size         = sizeof(uint64_t);
        m_value.v_uint = uint;
    }

    if (okptr)
        *okptr = ok;

    return uint;
}

double
Variant::to_double(bool* okptr) const
{
    double            d = m_value.v_double;
    cali_attr_type type = m_type;

    if (type == CALI_TYPE_INV && !m_string.empty()) {
        istringstream is(m_string.obj());

        is >> d;

        if (is)
            type = CALI_TYPE_DOUBLE;
    }

    bool ok = (type == CALI_TYPE_DOUBLE || type == CALI_TYPE_INT || type == CALI_TYPE_UINT);

    if (okptr)
        *okptr = ok;

    switch (type) {
    case CALI_TYPE_DOUBLE:
        return d;
    case CALI_TYPE_INT:
        return m_value.v_int;
    case CALI_TYPE_UINT:
        return m_value.v_uint;
    default:
        return 0;
    }
}

double
Variant::to_double(bool* okptr) 
{
    bool  ok = false;
    double d = const_cast<const Variant*>(this)->to_double(&ok);

    if (m_type == CALI_TYPE_INV && ok) {
        m_type           = CALI_TYPE_DOUBLE;
        m_size           = sizeof(double);
        m_value.v_double = d;
    }

    if (okptr)
        *okptr = ok;

    return d;
}

cali_attr_type
Variant::to_attr_type(bool* okptr) 
{
    if (m_type == CALI_TYPE_INV && !m_string.empty()) {
        m_value.v_type = cali_string2type(m_string.obj().c_str());

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
        return m_string.obj();

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
    {
        const char* str = static_cast<const char*>(m_value.ptr);
        std::size_t len = m_size;

        if (len && str[len-1] == 0)
            --len;
        
        ret.assign(str, len);
    }    
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

size_t
Variant::pack(unsigned char* buf) const
{
    size_t nbytes = 0;

    nbytes += vlenc_u64(static_cast<uint64_t>(m_type), buf);

    switch (m_type) {
    case CALI_TYPE_INV:
        break;    

    case CALI_TYPE_USR:
    case CALI_TYPE_STRING:
        nbytes += vlenc_u64(m_size, buf+nbytes);
    default:
        nbytes += vlenc_u64(m_value.v_uint, buf+nbytes);
        break;
    }

    return nbytes;
}

Variant
Variant::unpack(const unsigned char* buf, size_t* inc, bool *ok)
{
    size_t   p = 0;
    Variant  v;
    
    uint64_t u_type = vldec_u64(buf, &p);

    if (u_type > CALI_MAXTYPE) {
        if (ok)
            *ok = false;
        
        return v;
    }

    v.m_type = static_cast<cali_attr_type>(u_type);
        
    switch (v.m_type) {
    case CALI_TYPE_INV:
        break;

    case CALI_TYPE_USR:
    case CALI_TYPE_STRING:
        v.m_size         = static_cast<size_t>(vldec_u64(buf+p, &p));
    default:
        v.m_value.v_uint = vldec_u64(buf+p, &p);
    }

    if (inc)
        *inc += p;

    return v;
}

bool cali::operator == (const Variant& lhs, const Variant& rhs)
{
    if (lhs.m_type == CALI_TYPE_INV && rhs.m_type == CALI_TYPE_INV)
        return lhs.m_string == rhs.m_string;
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
    case CALI_TYPE_BOOL:
        return lhs.m_value.v_bool   == rhs.m_value.v_bool;
    case CALI_TYPE_TYPE:
        return lhs.m_value.v_type   == rhs.m_value.v_type;
    case CALI_TYPE_DOUBLE:
        return lhs.m_value.v_double == rhs.m_value.v_double;
    default:
        return lhs.m_value.v_uint   == rhs.m_value.v_uint;
    }
}

ostream& cali::operator << (ostream& os, const Variant& v)
{
    os << v.to_string();
    return os;
}

