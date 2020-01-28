// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \brief SnapshotBuffer.cc
/// SnapshotBuffer class definition

#include "caliper/common/SnapshotBuffer.h"

#include "caliper/common/CompressedSnapshotRecord.h"

#include <cstring>

using namespace cali;

unsigned char*
SnapshotBuffer::reserve(size_t min) {
    if (min <= m_reserved_len)
        return m_buffer;

    m_reserved_len = 4096 + min * 2;

    unsigned char* tmp = new unsigned char[m_reserved_len];
    memcpy(tmp, m_buffer, m_pos);
    delete[] m_buffer;
    m_buffer = tmp;

    return m_buffer;
}

SnapshotBuffer::SnapshotBuffer()
    : m_count(0),
      m_pos(0),
      m_reserved_len(0),
      m_buffer(0)
{ }

SnapshotBuffer::SnapshotBuffer(size_t size)
    : m_count(0),
      m_pos(0),
      m_reserved_len(size),
      m_buffer(new unsigned char[size])
{ }

SnapshotBuffer::~SnapshotBuffer() {
    delete[] m_buffer;
}

void
SnapshotBuffer::append(const CompressedSnapshotRecord& rec)
{
    memcpy(reserve(m_pos + rec.size()) + m_pos, rec.data(), rec.size());
    
    m_pos += rec.size();
    ++m_count;    
}
    

/// \brief Expose buffer for read from external source (e.g., MPI),
/// and set count and size.
unsigned char*
SnapshotBuffer::import(size_t size, size_t count)
{
    reserve(size);
        
    m_pos   = size;
    m_count = count;
        
    return m_buffer;
}

void
SnapshotBuffer::for_each(const std::function<void(const CompressedSnapshotRecordView&)> fn) const
{
    size_t pos = 0;
    
    for (size_t i = 0; i < m_count && pos < m_pos; ++i)
        fn(CompressedSnapshotRecordView(m_buffer + pos, &pos));
}
