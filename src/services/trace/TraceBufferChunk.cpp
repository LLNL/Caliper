// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "TraceBufferChunk.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

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
                
        uint64_t n_nodes = vldec_u64(m_data + p, &p);
        uint64_t n_attr  = vldec_u64(m_data + p, &p);

        rec.reserve(n_nodes + n_attr);

        for (size_t i = 0; i < n_nodes; ++i)
            rec.push_back(Entry(c->node(vldec_u64(m_data + p, &p))));
        for (size_t i = 0; i < n_attr;  ++i) {
            cali_id_t attr_id = vldec_u64(m_data + p, &p);
            Variant   data    = Variant::unpack(m_data + p, &p, nullptr);
            rec.push_back(Entry(attr_id, data));
        }
        
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


void TraceBufferChunk::save_snapshot(const SnapshotRecord* s)
{
    SnapshotRecord::Sizes sizes = s->size();

    if ((sizes.n_nodes + sizes.n_immediate) == 0)
        return;
                
    m_pos += vlenc_u64(sizes.n_nodes,     m_data + m_pos);
    m_pos += vlenc_u64(sizes.n_immediate, m_data + m_pos);

    SnapshotRecord::Data addr = s->data();

    for (size_t i = 0; i < sizes.n_nodes; ++i)
        m_pos += vlenc_u64(addr.node_entries[i]->id(), m_data + m_pos);
    for (size_t i = 0; i < sizes.n_immediate;  ++i) {
        m_pos += vlenc_u64(addr.immediate_attr[i], m_data + m_pos);
        m_pos += addr.immediate_data[i].pack(m_data + m_pos);
    }

    ++m_nrec;
}


bool TraceBufferChunk::fits(const SnapshotRecord* s) const
{
    SnapshotRecord::Sizes sizes = s->size();

    // get worst-case estimate of packed snapshot size:
    //   20 bytes for size indicators
    //   10 bytes per node id
    //   10+22 bytes per immediate entry (10 for attr, 22 for variant)
            
    size_t max = 20 + 10 * sizes.n_nodes + 32 * sizes.n_immediate;

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
