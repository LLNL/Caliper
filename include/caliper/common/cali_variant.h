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

#pragma once

/** \file  cali_variant.h
 *  \brief Caliper variant type definition
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "cali_types.h"

#include <stdbool.h>
#include <stddef.h>

/** The variant struct manages values of different types in Caliper.    
 *  Types with fixed size (i.e., numeric types) are stored in the variant directly.
 *  Variable-length types (strings and blobs) are stored as unmanaged pointers. 
 */    
typedef struct {
    /** Least significant bytes encode the type.
     *  Remaining bytes encode the size of variable-length types (strings and blobs (usr)).
     */
    uint64_t type_and_size;
    
    /** Value in various type representations
     */
    union {
        bool           v_bool;
        double         v_double;
        int            v_int;
        uint64_t       v_uint;
        cali_attr_type v_type;
        const void*    unmanaged_ptr;
    }        value;
} cali_variant_t;

#define CALI_VARIANT_TYPE_MASK 0xFF

inline cali_variant_t
cali_make_empty_variant()
{
    cali_variant_t v;
    v.type_and_size = 0;
    v.value.v_uint = 0;
    return v;
}
    
/** \brief Test if variant is empty
 */
inline bool
cali_variant_is_empty(cali_variant_t v)
{
    return 0 == v.type_and_size;
}
  
/** \brief Return type of a variant  
 */
cali_attr_type
cali_variant_get_type(cali_variant_t v);

/** \brief Return size of the variant's value
 */
size_t
cali_variant_get_size(cali_variant_t v);

/** \brief Get a pointer to the variant's data
 */
const void*
cali_variant_get_data(const cali_variant_t* v);

/** \brief Construct variant from type, pointer, and size 
 */
cali_variant_t
cali_make_variant(cali_attr_type type, const void* ptr, size_t size);


inline cali_variant_t
cali_make_variant_from_bool(bool value)
{
    cali_variant_t v;
    v.type_and_size = CALI_TYPE_BOOL;
    v.value.v_uint = 0;
    v.value.v_bool = value;
    return v;
}
    
inline cali_variant_t
cali_make_variant_from_int(int value)
{
    cali_variant_t v;
    v.type_and_size = CALI_TYPE_INT;
    v.value.v_uint = 0;
    v.value.v_int = value;    
    return v;
}

inline cali_variant_t
cali_make_variant_from_uint(uint64_t value)
{
    cali_variant_t v;
    v.type_and_size = CALI_TYPE_UINT;
    v.value.v_uint = value;
    return v;
}

inline cali_variant_t
cali_make_variant_from_double(double value)
{
    cali_variant_t v;
    v.type_and_size = CALI_TYPE_DOUBLE;
    v.value.v_double = value;
    return v;
}

inline cali_variant_t
cali_make_variant_from_type(cali_attr_type value)
{
    cali_variant_t v;
    v.type_and_size = CALI_TYPE_TYPE;
    v.value.v_uint = 0;
    v.value.v_type = value;
    return v;
}

/** \brief Return the variant's value as integer
 */
int
cali_variant_to_int(cali_variant_t v, bool* okptr);

uint64_t
cali_variant_to_uint(cali_variant_t v, bool* okptr);

double
cali_variant_to_double(cali_variant_t v, bool* okptr);

cali_attr_type
cali_variant_to_type(cali_variant_t v, bool* okptr);

bool
cali_variant_to_bool(cali_variant_t v, bool* okptr);
    
/** \brief Compare variant values. 
 */
int
cali_variant_compare(cali_variant_t lhs, cali_variant_t rhs);

bool
cali_variant_eq(cali_variant_t lhs, cali_variant_t rhs);

/** \brief Pack variant into byte buffer
 */
size_t
cali_variant_pack(cali_variant_t v, unsigned char* buf);    

/** \brief Unpack variant from byte buffer
 */
cali_variant_t
cali_variant_unpack(const unsigned char* buf, size_t* inc, bool* okptr);
    
#ifdef __cplusplus
} /* extern "C" */
#endif
