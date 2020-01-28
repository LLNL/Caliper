// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file  CompressedSnapshotRecord.h
/// \brief CompressedSnapshotRecord class

#pragma once

#include "caliper/common/Entry.h"
#include "caliper/common/Variant.h"

#include <vector>

namespace cali
{

class SnapshotRecord;
    
class CaliperMetadataAccessInterface;
class Node;

class CompressedSnapshotRecord;

//
// --- CompressedSnapshotRecordView
//
    
/// \brief A read-only decoder of a compressed snapshot record representation
///   at a given memory location
class CompressedSnapshotRecordView
{
    const unsigned char* m_buffer;

    size_t m_num_nodes;  ///< Number of node entries
    size_t m_num_imm;    ///< Number of immediate entries

    size_t m_imm_pos;    ///< Start position of immediate entries
    size_t m_imm_len;    ///< Length of immediate entries

    Entry  unpack_next_entry(const CaliperMetadataAccessInterface* c, size_t& n, size_t& pos);
    
    // CompressedSnapshotRecord can use this constructor to build an in-place view
    // without parsing the record representation first
    CompressedSnapshotRecordView(const unsigned char* buffer,
                                 size_t num_nodes,
                                 size_t num_imm,
                                 size_t imm_pos,
                                 size_t imm_len)
        : m_buffer(buffer),
          m_num_nodes(num_nodes),
          m_num_imm(num_imm),
          m_imm_pos(imm_pos),
          m_imm_len(imm_len)
    { }
    
public:

    constexpr CompressedSnapshotRecordView()
        : m_buffer(0),
          m_num_nodes(0),
          m_num_imm(0),
          m_imm_pos(0),
          m_imm_len(0)
    { }
          
    CompressedSnapshotRecordView(const unsigned char* m_buffer, size_t* inc);

    ~CompressedSnapshotRecordView()
    { }
    
    size_t num_nodes()      const { return m_num_nodes; }
    size_t num_immediates() const { return m_num_imm;   }

    /// \brief Unpack node entries
    void
    unpack_nodes(size_t n, cali_id_t node_vec[]) const;
    
    /// \brief Unpack immediate entries
    void
    unpack_immediate(size_t n, cali_id_t attr_vec[], Variant data_vec[]) const;
    
    std::vector<Entry>
    to_entrylist(const CaliperMetadataAccessInterface* c) const;

    template<class EntryProcFn>
    void
    unpack(const CaliperMetadataAccessInterface* c, EntryProcFn fn) {
        size_t n   = 0;
        size_t pos = 1;

        while (n < m_num_nodes + m_num_imm)
            if (!fn(unpack_next_entry(c, n, pos)))
                return;
    }

    friend class CompressedSnapshotRecord;
};


//
// --- CompressedSnapshotRecord
//

/// \brief CompressedSnapshotRecord encoder
class CompressedSnapshotRecord
{
    static constexpr size_t m_internal_buffer_size = 512;
    static constexpr size_t m_blocksize = 4;

    unsigned char  m_internal_buffer[m_internal_buffer_size];
    unsigned char* m_buffer;

    size_t m_buffer_len;

    size_t m_num_nodes;  ///< Number of node entries
    size_t m_num_imm;    ///< Number of immediate entries

    size_t m_imm_pos;    ///< Start position of immediate entries
    size_t m_imm_len;    ///< Length of immediate entries

    size_t m_needed_len; ///< Actually needed buffer space (may exceed given buffer len)
    size_t m_skipped;    ///< Number of entries skipped

public:

    CompressedSnapshotRecord(size_t len, unsigned char* buf);
    CompressedSnapshotRecord(size_t n, const Entry entrylist[]);
    
    CompressedSnapshotRecord();
    
    ~CompressedSnapshotRecord();

    size_t num_nodes()      const { return m_num_nodes; }
    size_t num_immediates() const { return m_num_imm;   }

    const unsigned char* data() const { return m_buffer; }

    size_t size() const {
        return std::min(m_imm_pos + m_imm_len, m_buffer_len);
    }

    size_t needed_len() const { return m_needed_len; }
    size_t num_skipped() const { return m_skipped; }

    /// \brief Append node entries
    size_t
    append(size_t n, const Node* const node_vec[]);
    
    /// \brief Append immediate entries
    size_t
    append(size_t n, const cali_id_t attr_vec[], const Variant data_vec[]);
    
    /// \brief Append entry list
    size_t
    append(size_t n, const Entry entrylist[]);

    size_t
    append(const SnapshotRecord* rec);
    
    CompressedSnapshotRecordView
    view() const {
        return CompressedSnapshotRecordView(m_buffer,
                                            m_num_nodes,
                                            m_num_imm,
                                            m_imm_pos,
                                            m_imm_len);
    }
};

} // namespace cali
