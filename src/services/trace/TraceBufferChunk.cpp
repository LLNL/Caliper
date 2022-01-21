// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "TraceBufferChunk.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"

#include "caliper/common/c-util/vlenc.h"

using namespace trace;
using namespace cali;


TraceBufferChunk::~TraceBufferChunk()
{
    delete[] m_data;

    if (m_next)
        delete m_next;
}


void TraceBufferChunk::append(TraceBufferChunk* chunk)
{
    if (m_next)
        m_next->append(chunk);
    else
        m_next = chunk;
}


void TraceBufferChunk::reset()
{
    m_pos  = 0;
    m_nrec = 0;

    memset(m_data, 0, m_size);

    delete m_next;
    m_next = 0;
}


size_t TraceBufferChunk::flush(Caliper* c, SnapshotFlushFn proc_fn)
{
    size_t written = 0;

    //
    // local flush
    //

    size_t p = 0;

    for (size_t r = 0; r < m_nrec; ++r) {
        // decode snapshot record
        std::vector<Entry> rec;

        uint64_t n = vldec_u64(m_data + p, &p);
        rec.reserve(n);

        while (n-- > 0)
            rec.push_back(Entry::unpack(*c, m_data + p, &p));

        // write snapshot
        proc_fn(*c, rec);
    }

    written += m_nrec;

    //
    // flush subsequent buffers in list
    //

    if (m_next)
        written += m_next->flush(c, proc_fn);

    return written;
}


void TraceBufferChunk::save_snapshot(SnapshotView s)
{
    if (s.empty())
        return;

    m_pos += vlenc_u64(s.size(), m_data + m_pos);

    for (const Entry& e : s)
        m_pos += e.pack(m_data + m_pos);

    ++m_nrec;
}


bool TraceBufferChunk::fits(SnapshotView rec) const
{
    // get worst-case estimate of packed snapshot size:
    //   10 bytes for size indicator
    //   n times Entry max size for data

    size_t max = 10 + rec.size() * Entry::MAX_PACKED_SIZE;

    return (m_pos + max) < m_size;
}


TraceBufferChunk::UsageInfo TraceBufferChunk::info() const
{
    UsageInfo info { 0, 0, 0 };

    if (m_next)
        info = m_next->info();

    info.nchunks++;
    info.reserved += m_size;
    info.used     += m_pos;

    return info;
}
