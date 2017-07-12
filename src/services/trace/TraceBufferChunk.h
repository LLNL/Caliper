// Copyright (c) 2016, Lawrence Livermore National Security, LLC.  
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

/// \file  TraceBufferChunk.h
/// \brief TraceBufferChunk class definition

#pragma once

#include "caliper/Caliper.h"

#include <cstring>


namespace cali
{
    class SnapshotRecord;
}

namespace trace
{

    class TraceBufferChunk {
        size_t            m_size;
        size_t            m_pos;
        size_t            m_nrec;
        
        unsigned char*    m_data;
        
        TraceBufferChunk* m_next;

    public:

        TraceBufferChunk(size_t s)
            : m_size(s), m_pos(0), m_nrec(0), m_data(new unsigned char[s]), m_next(0)
            { }

        ~TraceBufferChunk();

        void   append(TraceBufferChunk* chunk);
        void   reset();

        size_t flush(cali::Caliper* c, cali::Caliper::SnapshotFlushFn proc_fn);

        void   save_snapshot(const cali::SnapshotRecord* s);
        bool   fits(const cali::SnapshotRecord* s) const;

        struct UsageInfo {
            size_t nchunks;
            size_t reserved;
            size_t used;
        };

        UsageInfo info() const;
    };
    
} // namespace trace

