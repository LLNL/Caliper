/* Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
 * See top-level LICENSE file for details.
 */

#pragma once

/** \file  cali_variant.h
 *  \brief Caliper variant type definition
 */

#include "cali_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The variant struct manages values of different types in Caliper.
 *  Types with fixed size (i.e., numeric types) are stored in the variant directly.
 *  Variable-length types (strings and blobs) are stored as unmanaged pointers.
 */
typedef struct {
    /** Least significant byte encode the type.
     *  Top 4 bytes encode the size of variable-length types (strings and blobs).
     *  Remainder encodes a small hash for variable-length types for faster comparisons.
     */
    uint64_t type_and_size;

    /** Value in various type representations
     */
    union {
        uint64_t       v_uint; // let largest member be the first
        bool           v_bool;
        double         v_double;
        int64_t        v_int;
        cali_attr_type v_type;
        void*          unmanaged_ptr;
        const void*    unmanaged_const_ptr;
    }        value;
} cali_variant_t;

#define CALI_VARIANT_TYPE_MASK 0xFF
#define CALI_VARIANT_HASH_MASK 0xFFFFFF00

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
    v.value.v_int = value;
    return v;
}

inline cali_variant_t
cali_make_variant_from_int64(int64_t value)
{
    cali_variant_t v;
    v.type_and_size = CALI_TYPE_INT;
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
cali_make_variant_from_string(const char* value)
{
    size_t size = strlen(value);
    uint64_t hash = 0;

    const unsigned char* p = (const unsigned char*) value;
    hash |= p[0];
    hash <<= 8;
    hash |= p[size/2];
    hash <<= 8;
    hash |= p[size-1];
    hash <<= 8;

    cali_variant_t v;

    v.type_and_size = (size << 32) | hash | (CALI_TYPE_STRING & CALI_VARIANT_TYPE_MASK);
    v.value.unmanaged_const_ptr = value;

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

inline cali_variant_t
cali_make_variant_from_ptr(void* ptr)
{
    cali_variant_t v;
    v.type_and_size = CALI_TYPE_PTR;
    v.value.unmanaged_ptr = ptr;
    return v;
}

/** \brief Return the pointer stored in the variant. Only works for
 *    CALI_TYPE_PTR.
 */
inline void*
cali_variant_get_ptr(cali_variant_t v)
{
    return v.type_and_size == CALI_TYPE_PTR ? v.value.unmanaged_ptr : NULL;
}

/** \brief Return the variant's value as integer
 */
int
cali_variant_to_int(cali_variant_t v, bool* okptr);

int64_t
cali_variant_to_int64(cali_variant_t v, bool* okptr);

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

/** \brief Check if lhs and rhs' values are equal using a deep comparison.
 * Caller must make sure that lhs' and rhs' type and size are equal.
 */
bool
_cali_variant_value_eq(cali_variant_t lhs, cali_variant_t rhs);

/** \brief Check if lhs is equal to rhs
 */
inline bool
cali_variant_eq(cali_variant_t lhs, cali_variant_t rhs)
{
    return lhs.type_and_size == rhs.type_and_size ? _cali_variant_value_eq(lhs, rhs) : false;
}

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
