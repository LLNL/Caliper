// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \brief NodeBuffer.cc
/// NodeBuffer class definition

#include "caliper/common/NodeBuffer.h"

#include <caliper/common/Node.h>

#include <caliper/common/c-util/vlenc.h>

#include <cstring>

using namespace cali;

namespace
{
    inline NodeBuffer::NodeInfo make_nodeinfo(const Node* node) {
        cali_id_t parent_id = node->parent() ? node->parent()->id() : CALI_INV_ID;
        
        NodeBuffer::NodeInfo info = {
            node->id(),
            node->attribute(),
            parent_id,
            node->data()
        };

        return info;
    }
    
    inline size_t max_packed_size(const NodeBuffer::NodeInfo& info) {
        return 50 + info.value.size();
    }
    
    size_t pack_node(unsigned char* buf, const NodeBuffer::NodeInfo& info) {
        bool   have_parent = (info.parent_id != CALI_INV_ID);
        size_t pos = 0;

        // store id & flag in first uint: flag indicates whether parent ID is needed
        uint64_t u = 2 * info.node_id + (have_parent ? 1 : 0);

        pos += vlenc_u64(u, buf+pos);
        pos += vlenc_u64(info.attr_id, buf+pos);

        if (have_parent)
            pos += vlenc_u64(info.parent_id, buf+pos);

        // encode type and size and copy data directly for now
        pos += vlenc_u64(static_cast<uint64_t>(info.value.type()), buf+pos);
        pos += vlenc_u64(info.value.size(), buf+pos);
        memcpy(buf+pos, info.value.data(), info.value.size());

        pos += info.value.size();

        return pos;
    }
 
    NodeBuffer::NodeInfo unpack_node(const unsigned char* buf, size_t* inc) {
        NodeBuffer::NodeInfo ret = { CALI_INV_ID, CALI_INV_ID, CALI_INV_ID, Variant() };
        size_t   pos = 0;

        uint64_t u = vldec_u64(buf+pos, &pos);
        bool have_parent = ((u % 2) == 1);
        
        ret.node_id = u / 2;
        ret.attr_id = vldec_u64(buf+pos, &pos);
        
        if (have_parent)
            ret.parent_id = vldec_u64(buf+pos, &pos);
        
        u = vldec_u64(buf+pos, &pos);        
        size_t         size = vldec_u64(buf+pos, &pos);
        cali_attr_type type = (u <= CALI_MAXTYPE ? static_cast<cali_attr_type>(u) : CALI_TYPE_INV);

        ret.value = Variant(type, buf+pos, size);

        pos += size;
        *inc += pos;

        return ret;
    }
}

unsigned char*
NodeBuffer::reserve(size_t min) {
    if (min <= m_reserved_len)
        return m_buffer;

    m_reserved_len = 4096 + min * 2;

    unsigned char* tmp = new unsigned char[m_reserved_len];
    memcpy(tmp, m_buffer, m_pos);
    delete[] m_buffer;
    m_buffer = tmp;

    return m_buffer;
}

NodeBuffer::NodeBuffer()
    : m_count(0),
      m_pos(0),
      m_reserved_len(0),
      m_buffer(0)
{ }

NodeBuffer::NodeBuffer(size_t size)
    : m_count(0),
      m_pos(0),
      m_reserved_len(size),
      m_buffer(new unsigned char[size])
{ }

NodeBuffer::~NodeBuffer() {
    delete[] m_buffer;
}

void
NodeBuffer::append(const NodeInfo& info)
{
    reserve(m_pos + max_packed_size(info));

    m_pos += ::pack_node(m_buffer + m_pos, info);
    ++m_count;    
}

void
NodeBuffer::append(const Node* node)
{
    append(::make_nodeinfo(node));
}
    

/// \brief Expose buffer for read from external source (e.g., MPI),
/// and set count and size.
unsigned char*
NodeBuffer::import(size_t size, size_t count)
{
    reserve(size);
        
    m_pos   = size;
    m_count = count;
        
    return m_buffer;
}

void
NodeBuffer::for_each(const std::function<void(const NodeInfo&)> fn) const
{
    size_t pos = 0;
    
    for (size_t i = 0; i < m_count && pos < m_pos; ++i)
        fn(::unpack_node(m_buffer + pos, &pos));
}
