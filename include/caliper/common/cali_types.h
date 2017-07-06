/* *********************************************************************************************
 * Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
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

/** 
 * \file cali_types.h 
 * Context annotation library typedefs
 */

#ifndef CALI_CALI_TYPES_H
#define CALI_CALI_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t cali_id_t;

#define CALI_INV_ID 0xFFFFFFFFFFFFFFFF

typedef enum {
  CALI_TYPE_INV    = 0,
  CALI_TYPE_USR    = 1,
  CALI_TYPE_INT    = 2,
  CALI_TYPE_UINT   = 3,
  CALI_TYPE_STRING = 4,
  CALI_TYPE_ADDR   = 5,
  CALI_TYPE_DOUBLE = 6,
  CALI_TYPE_BOOL   = 7,
  CALI_TYPE_TYPE   = 8
} cali_attr_type;

#define CALI_MAXTYPE CALI_TYPE_TYPE

const char* 
cali_type2string(cali_attr_type);

cali_attr_type 
cali_string2type(const char*);

typedef enum {
  CALI_ATTR_DEFAULT       =  0,
  CALI_ATTR_ASVALUE       =  1,
  CALI_ATTR_NOMERGE       =  2,
  CALI_ATTR_SCOPE_PROCESS = 12, /* make scope flags mutually exclusive when &'ed with SCOPE_MASK */ 
  CALI_ATTR_SCOPE_THREAD  = 20, 
  CALI_ATTR_SCOPE_TASK    = 24,
  CALI_ATTR_SKIP_EVENTS   = 64,
  CALI_ATTR_HIDDEN        = 128
} cali_attr_properties;

#define CALI_ATTR_SCOPE_MASK 60

/**
 * Provides descriptive string of given attribute property flags, separated with ':'
 * \param  prop Attribute property flag
 * \param  buf  Buffer to write string to
 * \param  len  Length of string buffer
 * \return      -1 if provided buffer is too short; length of written string otherwise
 */  
int
cali_prop2string(int prop, char* buf, size_t len);

int
cali_string2prop(const char*);
  
typedef enum {
  CALI_OP_SUM = 1,
  CALI_OP_MIN = 2,
  CALI_OP_MAX = 3
} cali_op;

typedef enum {
  CALI_SUCCESS = 0,
  CALI_EBUSY,
  CALI_ELOCKED,
  CALI_EINV,
  CALI_ETYPE,
  CALI_ESTACK
} cali_err;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CALI_CALI_TYPES_H
