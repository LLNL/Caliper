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

#include "TraceBufferChunk.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/ContextRecord.h"
#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"

#include "caliper/common/c-util/vlenc.h"

#define SNAP_MAX 80

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
}


size_t TraceBufferChunk::flush(Caliper* c, Caliper::SnapshotProcessFn proc_fn)
{
    size_t written = 0;

    //
    // local flush
    //

    size_t p = 0;

    for (size_t r = 0; r < m_nrec; ++r) {
        // decode snapshot record
                
        int n_nodes = static_cast<int>(std::min(static_cast<int>(vldec_u64(m_data + p, &p)), SNAP_MAX));
        int n_attr  = static_cast<int>(std::min(static_cast<int>(vldec_u64(m_data + p, &p)), SNAP_MAX));

        SnapshotRecord::FixedSnapshotRecord<SNAP_MAX> snapshot_data;
        SnapshotRecord snapshot(snapshot_data);

        cali_id_t attr[SNAP_MAX];
        Variant   data[SNAP_MAX];

        for (int i = 0; i < n_nodes; ++i)
            snapshot.append(c->node(vldec_u64(m_data + p, &p)));
        for (int i = 0; i < n_attr;  ++i) 
            attr[i] = vldec_u64(m_data + p, &p);
        for (int i = 0; i < n_attr;  ++i)
            data[i] = Variant::unpack(m_data + p, &p, nullptr);

        snapshot.append(n_attr, attr, data);

        // write snapshot                

        proc_fn(&snapshot);
    }

    written += m_nrec;            
    reset();
            
    //
    // flush subsequent buffers in list
    // 
            
    if (m_next) {
        written += m_next->flush(c, proc_fn);
        delete m_next;
        m_next = 0;
    }
            
    return written;
}


void TraceBufferChunk::save_snapshot(const SnapshotRecord* s)
{
    SnapshotRecord::Sizes sizes = s->size();

    if ((sizes.n_nodes + sizes.n_immediate) == 0)
        return;

    sizes.n_nodes     = std::min<size_t>(sizes.n_nodes,     SNAP_MAX);
    sizes.n_immediate = std::min<size_t>(sizes.n_immediate, SNAP_MAX);
                
    m_pos += vlenc_u64(sizes.n_nodes,     m_data + m_pos);
    m_pos += vlenc_u64(sizes.n_immediate, m_data + m_pos);

    SnapshotRecord::Data addr = s->data();

    for (int i = 0; i < sizes.n_nodes; ++i)
        m_pos += vlenc_u64(addr.node_entries[i]->id(), m_data + m_pos);
    for (int i = 0; i < sizes.n_immediate;  ++i)
        m_pos += vlenc_u64(addr.immediate_attr[i],     m_data + m_pos);
    for (int i = 0; i < sizes.n_immediate;  ++i)
        m_pos += addr.immediate_data[i].pack(m_data + m_pos);

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
