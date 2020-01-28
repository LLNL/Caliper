// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file CompressedSnapshotRecord.cc
/// Caliper compressed snapshot record representation

#include "caliper/common/CompressedSnapshotRecord.h"

#include "caliper/SnapshotRecord.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Entry.h"
#include "caliper/common/Node.h"

#include "caliper/common/c-util/vlenc.h"

#include <algorithm>
#include <cstring>

using namespace cali;


namespace
{
    /// \brief Async-signal safe memory move.
    ///   Destination and source may overlap.
    inline void* save_memmove(void* dst, void* src, size_t len)
    {
        if (src == dst)
            return dst;

        while (len > 0) {
            unsigned char tmp[64];
            size_t blk = std::min<size_t>(len, 64);
            len -= blk;

            memcpy(tmp, static_cast<unsigned char*>(src)+len, blk);
            memcpy(static_cast<unsigned char*>(dst)+len, tmp, blk);
        }

        return dst;
    }
}

//
// --- CompressedSnapshotRecordView
//

CompressedSnapshotRecordView::CompressedSnapshotRecordView(const unsigned char* buf, size_t* inc)
    : m_buffer(buf)
{
    size_t pos = 0;
    
    m_num_nodes = buf[pos++];
    
    for (size_t i = 0; i < m_num_nodes; ++i)
        vldec_u64(buf+pos, &pos);

    m_imm_len = 0;
    m_imm_pos = pos;
    m_num_imm = buf[pos++];

    for (size_t i = 0; i < m_num_imm; ++i) {
        vldec_u64(buf+pos, &pos);
        Variant::unpack(buf+pos, &pos, nullptr);
    }

    m_imm_len = pos - m_imm_pos;
    *inc += pos;
}

Entry
CompressedSnapshotRecordView::unpack_next_entry(const CaliperMetadataAccessInterface* c, size_t& n, size_t& pos)
{
    if (n == 0)
        pos = 1;
    
    if (n < m_num_nodes) {
        ++n;
        return Entry(c->node(vldec_u64(m_buffer+pos, &pos)));
    }

    if (n == m_num_nodes)
        pos = m_imm_pos + 1;

    if (n < m_num_nodes + m_num_imm) {
        ++n;

        cali_id_t attr = vldec_u64(m_buffer+pos, &pos);
        Variant   data = Variant::unpack(m_buffer+pos, &pos, nullptr);

        return Entry(attr, data);
    }

    return Entry::empty;
}

/// \brief Unpack node entries
void
CompressedSnapshotRecordView::unpack_nodes(size_t n, cali_id_t node_vec[]) const
{
    size_t max = std::min(n, m_num_nodes);
    size_t pos = 1;

    for (size_t i = 0; i < max; ++i)
        node_vec[i] = vldec_u64(m_buffer+pos, &pos);
}

/// \brief Unpack immediate entries
void
CompressedSnapshotRecordView::unpack_immediate(size_t n, cali_id_t attr_vec[], Variant data_vec[]) const
{
    size_t max = std::min(n, m_num_imm);
    size_t pos = m_imm_pos + 1;

    for (size_t i = 0; i < max; ++i) {
        attr_vec[i] = vldec_u64(m_buffer+pos, &pos);
        data_vec[i] = Variant::unpack(m_buffer+pos, &pos, nullptr);
    }
}

std::vector<Entry>
CompressedSnapshotRecordView::to_entrylist(const CaliperMetadataAccessInterface* c) const
{
    std::vector<Entry> list;

    list.reserve(m_num_nodes + m_num_imm);

    {
        size_t pos = 1;

        for (size_t i = 0; i < m_num_nodes; ++i)
            list.push_back(Entry(c->node(vldec_u64(m_buffer+pos, &pos))));
    }

    {
        size_t pos = m_imm_pos + 1;

        for (size_t i = 0; i < m_num_imm; ++i) {
            cali_id_t attr = vldec_u64(m_buffer+pos, &pos);
            Variant   data = Variant::unpack(m_buffer+pos, &pos, nullptr);

            list.push_back(Entry(attr, data));
        }
    }

    return list;
}


//
// --- CompressedSnapshotRecord
//

CompressedSnapshotRecord::CompressedSnapshotRecord(size_t len, unsigned char* buf)
    : m_buffer(buf),
      m_buffer_len(len),
      m_num_nodes(0),
      m_num_imm(0),
      m_imm_pos(1),
      m_imm_len(1),
      m_needed_len(2),
      m_skipped(0)
{
    memset(m_buffer, 0, m_buffer_len);
}

CompressedSnapshotRecord::CompressedSnapshotRecord()
    : CompressedSnapshotRecord(m_internal_buffer_size, m_internal_buffer)
{ }

CompressedSnapshotRecord::CompressedSnapshotRecord(size_t n, const Entry entrylist[])
    : CompressedSnapshotRecord()
{
    append(n, entrylist);
}

CompressedSnapshotRecord::~CompressedSnapshotRecord()
{ }

/// \brief Append node entries
size_t
CompressedSnapshotRecord::append(size_t n, const Node* const node_vec[])
{
    size_t skipped = 0;

    // blockwise encode, size check, and copy
    while (n > 0) {
        unsigned char tmp[m_blocksize*10];
        size_t blk = std::min(n, m_blocksize);
        size_t len = 0;

        // encode to temp buffer
        for (size_t i = 0; i < blk; ++i)
            len += vlenc_u64(node_vec[i]->id(), tmp+len);

        // size check, copy to actual buffer
        if (m_num_nodes+blk < 128 && m_imm_pos+m_imm_len+len <= m_buffer_len) {
            ::save_memmove(m_buffer+m_imm_pos+len, m_buffer+m_imm_pos, m_imm_len);
            memcpy(m_buffer+1, tmp, len);

            m_imm_pos   += len;
            m_num_nodes += blk;
            m_buffer[0] += blk;
        } else {
            skipped     += blk;
        }

        m_needed_len += len;

        // advance
        node_vec    += blk;
        n           -= blk;
    }

    m_skipped += skipped;

    return skipped;
}

/// \brief Append immediate entries
size_t
CompressedSnapshotRecord::append(size_t n, const cali_id_t attr_vec[], const Variant data_vec[])
{
    size_t skipped = 0;

    // blockwise encode, size check, and copy
    while (n > 0) {
        unsigned char tmp[m_blocksize*30]; // holds immediate entries (10 bytes per id + 20 bytes per variant)
        size_t blk = std::min(n, m_blocksize);
        size_t len = 0;

        // encode to tmp buffer
        for (size_t i = 0; i < blk; ++i) {
            len += vlenc_u64(attr_vec[i], tmp+len);
            len += data_vec[i].pack(tmp+len);
        }

        // size check, copy to actual buffer
        if (m_num_imm+blk < 128 && m_imm_pos+m_imm_len+len <= m_buffer_len) {
            memcpy(m_buffer+m_imm_pos+m_imm_len, tmp, len);

            m_imm_len += len;
            m_num_imm += blk;
            m_buffer[m_imm_pos] += blk;
        } else {
            skipped   += blk;
        }

        m_needed_len += len;

        // advance
        attr_vec += blk;
        data_vec += blk;
        n        -= blk;
    }

    m_skipped += skipped;

    return skipped;
}

/// \brief Append entry list
size_t
CompressedSnapshotRecord::append(size_t n, const Entry entrylist[]) 
{
    size_t skipped = 0;

    const Node* nodes[m_blocksize];
    cali_id_t    attr[m_blocksize];
    Variant      data[m_blocksize];

    size_t nn = 0;
    size_t ni = 0;

    for (size_t p = 0; p < n; ++p) {
        const Entry& e(entrylist[p]);

        if (e.node()) {
            nodes[nn] = e.node();

            if (++nn == m_blocksize) {
                skipped += append(nn, nodes);
                nn = 0;
            }
        } else if (e.is_immediate()) {
            attr[ni] = e.attribute();
            data[ni] = e.value();

            if (++ni == m_blocksize) {
                skipped += append(ni, attr, data);
                ni = 0;
            }
        }
    }

    skipped += append(nn, nodes);
    skipped += append(ni, attr, data);

    return skipped;
}

/// \brief Append snapshot record
size_t
CompressedSnapshotRecord::append(const SnapshotRecord* rec)
{
    size_t skipped = 0;

    SnapshotRecord::Sizes size = rec->size();
    SnapshotRecord::Data  data = rec->data();

    skipped += append(size.n_nodes, data.node_entries);
    skipped += append(size.n_immediate, data.immediate_attr, data.immediate_data);

    return skipped;
}

constexpr size_t CompressedSnapshotRecord::m_blocksize;
