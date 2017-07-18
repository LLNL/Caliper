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
    size_t         m_count;
    
    size_t         m_pos;
    size_t         m_reserved_len;
    
    unsigned char* m_buffer;

    unsigned char* reserve(size_t min);
    
public:

    SnapshotBuffer();
    SnapshotBuffer(size_t size);
    
    ~SnapshotBuffer();
    
    SnapshotBuffer(const SnapshotBuffer&) = delete;
    SnapshotBuffer& operator = (const SnapshotBuffer&) = delete;

    void append(const CompressedSnapshotRecord& rec);

    size_t count() const { return m_count; }
    size_t size() const  { return m_pos;   }
    
    const unsigned char* data() const { return m_buffer; }

    /// \brief Expose buffer for read from external source (e.g., MPI),
    /// and set count and size.
    unsigned char* import(size_t size, size_t count);

    /// \brief Run function \a fn on each snapshot  
    void for_each(std::function<void(const CompressedSnapshotRecordView&)> fn) const;
};

} // namespace cali
