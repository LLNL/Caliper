/* Copyright (c) 2016, David Boehme.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*! \file  unitfmt.h
 *  \brief Scale values to human-readable size
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct unitfmt_table{
    unsigned long long factor;
    const char*  symbol;  
};

/*! \brief A format table for byte values (in base-2) */
extern const struct unitfmt_table unitfmt_bytes[];

typedef struct {
    double      val;
    const char* symbol;
} unitfmt_result;

    
/*! \brief Scale values to human-readable size
 * 
 * \param val    Value to scale
 * \param table  Reference table with unit conversion factors and symbols
 *
 * \return       Conversion result. Member val is the scaled value, member 
 *               symbol points to the conversion factor symbol.
 */    
unitfmt_result
unitfmt(unsigned long long val, const struct unitfmt_table table[]);
    
#ifdef __cplusplus
} /* extern "C" */
#endif
