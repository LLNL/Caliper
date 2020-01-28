// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file  SnapshotBuffer.h
/// \brief SnapshotBuffer class 

#pragma once

#include <functional>

namespace cali
{

class CompressedSnapshotRecord;
class CompressedSnapshotRecordView;
    
/// \brief Serialize/deserialize a set of nodes
class SnapshotBuffer
{
    std::size_t         m_count;
    
    std::size_t         m_pos;
    std::size_t         m_reserved_len;
    
    unsigned char* m_buffer;

    unsigned char* reserve(std::size_t min);
    
public:

    SnapshotBuffer();
    SnapshotBuffer(std::size_t size);
    
    ~SnapshotBuffer();
    
    SnapshotBuffer(const SnapshotBuffer&) = delete;
    SnapshotBuffer& operator = (const SnapshotBuffer&) = delete;

    void append(const CompressedSnapshotRecord& rec);

    std::size_t count() const { return m_count; }
    std::size_t size() const  { return m_pos;   }
    
    const unsigned char* data() const { return m_buffer; }

    /// \brief Expose buffer for read from external source (e.g., MPI),
    /// and set count and size.
    unsigned char* import(std::size_t size, std::size_t count);

    /// \brief Run function \a fn on each snapshot  
    void for_each(std::function<void(const CompressedSnapshotRecordView&)> fn) const;
};

} // namespace cali
