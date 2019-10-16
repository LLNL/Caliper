// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file NodeBuffer.h
/// \brief NodeBuffer class 

#pragma once

#include <caliper/common/Variant.h>

#include <functional>

namespace cali
{

class Node;
    
/// \brief Serialize/deserialize a set of nodes
class NodeBuffer
{
    size_t         m_count;
    
    size_t         m_pos;
    size_t         m_reserved_len;
    
    unsigned char* m_buffer;

    unsigned char* reserve(size_t min);
    
public:

    struct NodeInfo {
        cali_id_t node_id;
        cali_id_t attr_id;
        cali_id_t parent_id;
        Variant   value;
    };    

    NodeBuffer();
    NodeBuffer(size_t size);
    
    ~NodeBuffer();
    
    NodeBuffer(const NodeBuffer&) = delete;
    NodeBuffer& operator = (const NodeBuffer&) = delete;

    void append(const NodeInfo& info);
    void append(const Node* node);

    size_t count() const { return m_count; }
    size_t size() const  { return m_pos;   }
    
    const unsigned char* data() const { return m_buffer; }

    /// \brief Expose buffer for read from external source (e.g., MPI),
    /// and set count and size.
    unsigned char* import(size_t size, size_t count);

    /// \brief Run function \a fn on each element  
    void for_each(std::function<void(const NodeInfo&)> fn) const;
};

} // namespace cali
