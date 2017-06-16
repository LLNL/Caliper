/* *********************************************************************************************
 * Copyright (c) 2017, Lawrence Livermore National Security, LLC.  
 * Produced at the Lawrence Livermore National Laboratory.
 *
 * This file is part of Caliper.
 * Written by David Boehme, boehme3@llnl.gov.
 * LLNL-CODE-678900
 * All rights reserved.
 *
 * For details, see https://github.com/scalability-llnl/Caliper.
 * Please also see the LICENSE file for our additional BSD notice.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the disclaimer below.
 *  * Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions and the disclaimer (as noted below) in the documentation and/or other materials
 *    provided with the distribution.
 *  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * *********************************************************************************************/

/** \file  cali_variant.c
 *  \brief cali_variant_t definition
 */

#include "caliper/common/cali_variant.h"

#include "caliper/common/c-util/vlenc.h"

#include <string.h>

#define _EXTRACT_TYPE(type_and_size) ((type_and_size) & CALI_VARIANT_TYPE_MASK)
#define _EXTRACT_SIZE(type_and_size) ((type_and_size) >> 8)

extern inline bool
cali_variant_is_empty(cali_variant_t v);

extern inline cali_variant_t
cali_make_empty_variant();

cali_attr_type
cali_variant_get_type(cali_variant_t v)
{
    uint64_t t = _EXTRACT_TYPE(v.type_and_size);
    return (t <= CALI_MAXTYPE ? (cali_attr_type) t : CALI_TYPE_INV);
}

size_t
cali_variant_get_size(cali_variant_t v)
{
    uint64_t          t = _EXTRACT_TYPE(v.type_and_size);
    cali_attr_type type = (t <= CALI_MAXTYPE ? (cali_attr_type) t : CALI_TYPE_INV);

    switch (type) {
    case CALI_TYPE_INV:
        return 0;
    case CALI_TYPE_USR:
    case CALI_TYPE_STRING:
        return _EXTRACT_SIZE(v.type_and_size);
    case CALI_TYPE_INT:
        return sizeof(int);
    case CALI_TYPE_UINT:
    case CALI_TYPE_ADDR:
        return sizeof(uint64_t);
    case CALI_TYPE_DOUBLE:
        return sizeof(double);
    case CALI_TYPE_BOOL:
        return sizeof(bool);
    case CALI_TYPE_TYPE:
        return sizeof(cali_attr_type);
    }

    return 0;
}

const void*
cali_variant_get_data(const cali_variant_t* v)
{
    uint64_t          t = _EXTRACT_TYPE(v->type_and_size);
    cali_attr_type type = (t <= CALI_MAXTYPE ? (cali_attr_type) t : CALI_TYPE_INV);
    
    switch (type) {
    case CALI_TYPE_INV:
        return 0;
    case CALI_TYPE_USR:
    case CALI_TYPE_STRING:
        return v->value.unmanaged_ptr;
    case CALI_TYPE_INT:
        return &v->value.v_int;
    case CALI_TYPE_ADDR:
    case CALI_TYPE_UINT:
        return &v->value.v_uint;
    case CALI_TYPE_DOUBLE:
        return &v->value.v_double;
    case CALI_TYPE_BOOL:
        return &v->value.v_bool;
    case CALI_TYPE_TYPE:
        return &v->value.v_type;
    }

    return NULL;
}


cali_variant_t
cali_make_variant(cali_attr_type type, const void* ptr, size_t size)
{
    cali_variant_t v = { 0, { .v_uint = 0 } };

    v.type_and_size = (type & CALI_VARIANT_TYPE_MASK);
    
    switch (type) {
    case CALI_TYPE_INV:
        break;
    case CALI_TYPE_USR:
    case CALI_TYPE_STRING:
        v.type_and_size  = (size << 8) | (type & CALI_VARIANT_TYPE_MASK);
        v.value.unmanaged_ptr = ptr;
        break;
    case CALI_TYPE_INT:
        v.value.v_int    = *((const int*) ptr);
        break;
    case CALI_TYPE_ADDR:
    case CALI_TYPE_UINT:
        v.value.v_uint   = *((const uint64_t*) ptr);
        break;
    case CALI_TYPE_DOUBLE:
        v.value.v_double = *((const double*) ptr);
        break;
    case CALI_TYPE_BOOL:
        v.value.v_bool   = *((const bool*) ptr);
        break;
    case CALI_TYPE_TYPE:
        v.value.v_type   = *((const cali_attr_type*) ptr);
        break;
    }

    return v;
}
                  
extern inline cali_variant_t
cali_make_variant_from_bool(bool value);

extern inline cali_variant_t
cali_make_variant_from_int(int value);

extern inline cali_variant_t
cali_make_variant_from_uint(uint64_t value);

extern inline cali_variant_t
cali_make_variant_from_double(double value);

extern inline cali_variant_t
cali_make_variant_from_type(cali_attr_type value);


/** \brief Return the variant's value as integer
 *
 *  This function returns the variant's value as integer.
 *  Numeric types (bool, int, uint, double, type) will be converted to int,
 *  and value pointed to by \a okptr is set to `true`.
 *  For other types (blobs and strings) the function returns zero, and the 
 *  value pointed to by \a okptr is set to `false`.
 *  
 *  \param v     The input variant
 *  \param okptr If non-NULL, indicate success or failure int the referenced variable.   
 *  \return The integer value. Zero if the conversion was unsuccesful.
 */
int
cali_variant_to_int(cali_variant_t v, bool* okptr)
{
    bool ok  = true;
    int  ret = 0;

    uint64_t       t    = _EXTRACT_TYPE(v.type_and_size);
    cali_attr_type type = (t <= CALI_MAXTYPE ? (cali_attr_type) t : CALI_TYPE_INV);

    switch (type) {
    case CALI_TYPE_INV:
    case CALI_TYPE_USR:
    case CALI_TYPE_STRING:
        ok  = false;
        break;
    case CALI_TYPE_INT:
        ret = v.value.v_int;
        break;
    case CALI_TYPE_ADDR:
    case CALI_TYPE_UINT:
        ret = (int) v.value.v_uint;
        break;
    case CALI_TYPE_DOUBLE:
        ret = (int) v.value.v_double;
        break;
    case CALI_TYPE_BOOL:
        ret = (int) v.value.v_bool;
        break;
    case CALI_TYPE_TYPE:
        ret = (int) v.value.v_type;
        break;
    }

    if (okptr)
        *okptr = ok;

    return ret;
}

/** \brief Return the variant's value as unsigned integer
 *
 *  This function returns the variant's value as integer.
 *  Numeric types (bool, int, uint, double, type) will be converted to `uint64_t`,
 *  and value pointed to by \a okptr is set to `true`.
 *  For other types (blobs and strings) the function returns zero, and the 
 *  value pointed to by \a okptr is set to `false`.
 *  
 *  \param v     The input variant
 *  \param okptr If non-NULL, indicate success or failure int the referenced variable.   
 *  \return The integer value. Zero if the conversion was unsuccesful.
 */
uint64_t
cali_variant_to_uint(cali_variant_t v, bool* okptr)
{
    bool     ok  = true;
    uint64_t ret = 0;

    uint64_t       t    = _EXTRACT_TYPE(v.type_and_size);
    cali_attr_type type = (t <= CALI_MAXTYPE ? (cali_attr_type) t : CALI_TYPE_INV);

    switch (type) {
    case CALI_TYPE_INV:
    case CALI_TYPE_USR:
    case CALI_TYPE_STRING:
        ok  = false;
        break;
    case CALI_TYPE_INT:
        ret = (uint64_t) v.value.v_int;
        break;
    case CALI_TYPE_ADDR:
    case CALI_TYPE_UINT:
        ret = v.value.v_uint;
        break;
    case CALI_TYPE_DOUBLE:
        ret = (uint64_t) v.value.v_double;
        break;
    case CALI_TYPE_BOOL:
        ret = (uint64_t) v.value.v_bool;
        break;
    case CALI_TYPE_TYPE:
        ret = (uint64_t) v.value.v_type;
        break;
    }

    if (okptr)
        *okptr = ok;

    return ret;
}

/** \brief Return the variant's value as double
 *
 *  This function returns the variant's value as double.
 *  Numeric types (bool, int, uint, double, type) will be converted to double,
 *  and value pointed to by \a okptr is set to `true`.
 *  For other types (blobs and strings) the function returns zero, and the 
 *  value pointed to by \a okptr is set to `false`.
 *  
 *  \param v     The input variant
 *  \param okptr If non-NULL, indicate success or failure int the referenced variable.   
 *  \return The double value. Zero if the conversion was unsuccesful.
 */
double
cali_variant_to_double(cali_variant_t v, bool* okptr)
{
    bool   ok  = true;
    double ret = 0;

    uint64_t          t = _EXTRACT_TYPE(v.type_and_size);
    cali_attr_type type = (t <= CALI_MAXTYPE ? (cali_attr_type) t : CALI_TYPE_INV);

    switch (type) {
    case CALI_TYPE_INV:
    case CALI_TYPE_USR:
    case CALI_TYPE_STRING:
        ok  = false;
        break;
    case CALI_TYPE_INT:
        ret = (double) v.value.v_int;
        break;
    case CALI_TYPE_ADDR:
    case CALI_TYPE_UINT:
        ret = (double) v.value.v_uint;
        break;
    case CALI_TYPE_DOUBLE:
        ret = v.value.v_double;
        break;
    case CALI_TYPE_BOOL:
        ret = (double) v.value.v_bool;
        break;
    case CALI_TYPE_TYPE:
        ret = (double) v.value.v_type;
        break;
    }

    if (okptr)
        *okptr = ok;

    return ret;
}

/** \brief Return the variant's value as boolean
 *
 *  This function returns the variant's value as type.
 *  For boolean and integer types (int, uint, addr, bool), the returned
 *  value is `false` if the value is equal to 0, and `true` otherwise.
 *  For other types the function returns `false`, and the 
 *  value pointed to by \a okptr is set to `false`.
 *  
 *  \param v     The input variant
 *  \param okptr If non-NULL, indicate success or failure int the referenced variable.   
 *  \return The boolean value; `false` if the conversion was unsuccesful.
 */
bool
cali_variant_to_bool(cali_variant_t v, bool* okptr)
{
    bool ok  = true;
    bool ret = false;

    uint64_t          t = _EXTRACT_TYPE(v.type_and_size);
    cali_attr_type type = (t <= CALI_MAXTYPE ? (cali_attr_type) t : CALI_TYPE_INV);

    switch (type) {
    case CALI_TYPE_INT:
        ret = (v.value.v_int  != 0);
        break;
    case CALI_TYPE_UINT:
    case CALI_TYPE_ADDR:
        ret = (v.value.v_uint != 0);
        break;
    case CALI_TYPE_BOOL:
        ret = v.value.v_bool;
        break;
    default:
        ok  = false;
    }

    if (okptr)
        *okptr = ok;

    return ret;
}

/** \brief Return the variant's value as type
 *
 *  This function returns the variant's value as type.
 *  This is only successful for TYPE variants.
 *  For other types the function returns CALI_TYPE_INV, and the 
 *  value pointed to by \a okptr is set to `false`.
 *  
 *  \param v     The input variant
 *  \param okptr If non-NULL, indicate success or failure int the referenced variable.   
 *  \return The type value. CALI_TYPE_INV if the conversion was unsuccesful.
 */
cali_attr_type
cali_variant_to_type(cali_variant_t v, bool* okptr)
{
    bool ok  = true;
    cali_attr_type ret = CALI_TYPE_INV;

    uint64_t          t = _EXTRACT_TYPE(v.type_and_size);
    cali_attr_type type = (t <= CALI_MAXTYPE ? (cali_attr_type) t : CALI_TYPE_INV);

    switch (type) {
    case CALI_TYPE_TYPE:
        ret = v.value.v_type;
        break;
    default:
        ok  = false;
    }

    if (okptr)
        *okptr = ok;

    return ret;
}

static inline int
imin(int l, int r)
{
    return l < r ? l : r;
}

int
cali_variant_compare(cali_variant_t lhs, cali_variant_t rhs)
{
    uint64_t t = _EXTRACT_TYPE(lhs.type_and_size);
    cali_attr_type lhstype = (t <= CALI_MAXTYPE ? (cali_attr_type) t : CALI_TYPE_INV);
    t          = _EXTRACT_TYPE(rhs.type_and_size);
    cali_attr_type rhstype = (t <= CALI_MAXTYPE ? (cali_attr_type) t : CALI_TYPE_INV);

    if (lhstype == rhstype) {
        switch (lhstype) {
        case CALI_TYPE_INV:
            return lhs.value.v_int - rhs.value.v_int;
        case CALI_TYPE_USR:
            {
                int lhssize = (int) _EXTRACT_SIZE(lhs.type_and_size);
                int rhssize = (int) _EXTRACT_SIZE(rhs.type_and_size);
                int cmp     = memcmp(lhs.value.unmanaged_ptr, rhs.value.unmanaged_ptr,
                                     imin(lhssize, rhssize));

                return (cmp ? cmp : (lhssize - rhssize));
            }
        case CALI_TYPE_STRING:
            {
                int lhssize = (int) _EXTRACT_SIZE(lhs.type_and_size);
                int rhssize = (int) _EXTRACT_SIZE(rhs.type_and_size);                
                int cmp     = strncmp(lhs.value.unmanaged_ptr, rhs.value.unmanaged_ptr,
                                      imin(lhssize, rhssize));

                return (cmp ? cmp : (lhssize - rhssize));
            }
        case CALI_TYPE_INT:
            return lhs.value.v_int - rhs.value.v_int;
        case CALI_TYPE_ADDR:
        case CALI_TYPE_UINT:
            {
                uint64_t diff = lhs.value.v_uint - rhs.value.v_uint;
                return (diff == 0 ? 0 : (diff > lhs.value.v_uint ? -1 : 1));
            }
        case CALI_TYPE_DOUBLE:
            {
                double diff = lhs.value.v_double - rhs.value.v_double;
                return (diff < 0.0 ? -1 : (diff > 0.0 ? 1 : 0));
            }
        case CALI_TYPE_BOOL:
            {
                return (lhs.value.v_bool ? 1 : 0) - (rhs.value.v_bool ? 1 : 0);
            }
        case CALI_TYPE_TYPE:
            return ((int) lhs.value.v_type) - ((int) rhs.value.v_type);
        }
    }

    return ((int) lhstype) - ((int) rhstype);
}

bool
cali_variant_eq(cali_variant_t lhs, cali_variant_t rhs)
{
    if (lhs.type_and_size != rhs.type_and_size)
        return false;    

    switch ( _EXTRACT_TYPE(lhs.type_and_size) ) {
    case CALI_TYPE_USR:
    case CALI_TYPE_STRING:
        {
            if (lhs.value.unmanaged_ptr == rhs.value.unmanaged_ptr)
                return true;
            else
                return 0 == memcmp(lhs.value.unmanaged_ptr, rhs.value.unmanaged_ptr, 
                                   _EXTRACT_SIZE(lhs.type_and_size));
        }
        break;
    default:
        return lhs.value.v_uint == rhs.value.v_uint;
    }

    return false;
}

size_t
cali_variant_pack(cali_variant_t v, unsigned char* buf)
{
    size_t pos = 0;

    pos += vlenc_u64(v.type_and_size, buf);
    pos += vlenc_u64(v.value.v_uint,  buf+pos);

    return pos;
}

cali_variant_t
cali_variant_unpack(const unsigned char* buf, size_t* inc, bool *okptr)
{
    cali_variant_t v = { 0, { .v_uint = 0 } };
    size_t p = 0;

    uint64_t ts = vldec_u64(buf, &p);
    
    if (_EXTRACT_TYPE(ts) > CALI_MAXTYPE) {
        if (okptr)
            *okptr = false;

        return v;
    }

    v.type_and_size = ts;
    v.value.v_uint  = vldec_u64(buf+p, &p);
    
    if (inc)
        *inc  += p;
    if (okptr)
        *okptr = true; 

    return v;
}

