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

/*! \file  vlenc.h
 *  \brief Variable-length integer encoding
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! \brief Write 64-bit value into buffer using variable-length encoding.
 *  
 * \param  val  64-bit unsigned integer value to be written.
 * \param  buf  Character buffer. Must be large enough to hold 10 bytes.
 * \return      Number of bytes written.
 */
inline size_t
vlenc_u64(uint64_t val, unsigned char* buf)
{
    size_t nbytes = 1;

    *buf++ = val & 0x7F;

    while (val > 0x7F) {
        val    >>= 7;
        *buf++   = (val & 0x7F) | 0x80;
        ++nbytes;
    }

    return nbytes;
}

/*! \brief Read variable-length encoded 64-bit value from buffer.
 *
 * \param  buf  Buffer to read from.
 * \param  inc  The function increments this value by the number of bytes read.
 * \return      The decoded value.
 */    
inline uint64_t
vldec_u64(const unsigned char* buf, size_t* inc)
{
    uint64_t val = buf[0] & 0x7F;
    size_t   p   = 1;

    for ( ; p < 10 && (buf[p] & 0x80); ++p)
        val |= ((uint64_t) (buf[p] & 0x7F) << (7*p));
    
    if (inc)
        *inc += p;
    
    return val;
}
    
#ifdef __cplusplus
} /* extern "C" */
#endif
