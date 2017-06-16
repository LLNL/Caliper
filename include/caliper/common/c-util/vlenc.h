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
 *
 *  This module contains functions to pack and unpack integer values
 *  using variable-length encoding. Variable-length encoding reduces the
 *  amount of bytes required to store small-to-medium values. Values
 *  smaller than 128 take up only one byte. However, for very large
 *  values, a few extra bytes may be consumed.
 *
 *  Encoding:
 *
 *  ~~~~~~~~{.c}
 *    uint64_t val1 = 1, val2 = 42;
 *    // Worst-case encoded size for one 64-bit integer is 10 bytes.
 *    // A 20-byte buffer will hold 2 values.
 *    unsigned char buf[20];
 *    size_t pos = 0;
 *
 *    // Encode values and advance the buffer positon
 *    pos += vlenc_u64(val1, buf);
 *    pos += vlenc_u64(val2, buf+pos);
 *
 *    // "pos" now contains the length of the two encoded values
 *  ~~~~~~~~
 *
 *  Decoding:
 *
 *  ~~~~~~~~{.c}
 *    uint64_t val1, val2;
 *    size_t pos = 0;
 *
 *    // Read two values.
 *    // vldec_u64() increases "pos" by the number of bytes read.
 *    val1 = vldec_u64(buf+pos, &pos);
 *    val2 = vldec_u64(buf+pos, &pos);
 *  ~~~~~~~~
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! \brief Write 64-bit value into \a buf using variable-length encoding.
 *
 * \param  val  64-bit unsigned integer value to be written.
 * \param  buf  Character buffer. Must be large enough to hold 10 bytes.
 * \return      Number of bytes written.
 */
inline size_t
vlenc_u64(uint64_t val, unsigned char* buf)
{
    size_t nbytes = 0;

    while (val > 0x7F) {
        *buf++   = (val & 0x7F) | 0x80;
        val    >>= 7;
        ++nbytes;
    }

    *buf++ = val;
    ++nbytes;

    return nbytes;
}

/*! \brief Read variable-length encoded 64-bit value from \a buf.
 *
 * The function reads a variable-length encoded 64-bit unsigned integer value
 * from \a buf and increments \a inc by the number of bytes read.
 *
 * \param  buf  Buffer to read from.
 * \param  inc  The function increments this value by the number of bytes read.
 * \return      The decoded value.
 */
inline uint64_t
vldec_u64(const unsigned char* buf, size_t* inc)
{
    uint64_t val = 0;
    size_t   p   = 0;

    for ( ; p < 9 && (buf[p] & 0x80); ++p)
        val |= ((uint64_t) (buf[p] & 0x7F) << (7*p));

    val |= ((uint64_t) (buf[p] & 0x7F) << (7*p));
    ++p;

    if (inc)
        *inc += p;

    return val;
}

#ifdef __cplusplus
} /* extern "C" */
#endif
