// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#pragma once

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include <cstring>

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

        size_t flush(cali::Caliper* c, cali::SnapshotFlushFn proc_fn);

        void   save_snapshot(cali::SnapshotView s);
        bool   fits(cali::SnapshotView s) const;

        struct UsageInfo {
            size_t nchunks;
            size_t reserved;
            size_t used;
        };

        UsageInfo info() const;
    };
} // namespace trace
