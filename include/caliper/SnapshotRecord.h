// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file SnapshotRecord.h
/// \brief Snapshot record representation.

#pragma once

#include "common/Attribute.h"
#include "common/Entry.h"

#include <algorithm>
#include <map>
#include <vector>

namespace cali
{

class CaliperMetadataAccessInterface;

// Snapshots are fixed-size, stack-allocated objects that can be used in 
// a signal handler  

/// \brief Snapshot record representation.

class SnapshotRecord 
{    
public:

    struct Data {
        const cali::Node* const* node_entries;
        const cali_id_t*     immediate_attr;
        const cali::Variant* immediate_data;
    };

    struct Sizes {
        std::size_t n_nodes;
        std::size_t n_immediate;
    };

    template<std::size_t N>
    struct FixedSnapshotRecord {
        cali::Node*   node_array[N];
        cali_id_t     attr_array[N];
        cali::Variant data_array[N];

        FixedSnapshotRecord() {
            std::fill_n(node_array, N, nullptr);
            std::fill_n(attr_array, N, CALI_INV_ID);
            std::fill_n(data_array, N, cali::Variant());            
        }
    };

    SnapshotRecord()
        : m_node_array { 0 },
          m_attr_array { 0 },
          m_data_array { 0 },
          m_sizes    { 0, 0 },
          m_capacity { 0, 0 }
        { }
    
    template<std::size_t N>
    SnapshotRecord(FixedSnapshotRecord<N>& list)
        : m_node_array { list.node_array },
          m_attr_array { list.attr_array },
          m_data_array { list.data_array },
          m_sizes    { 0, 0 },
          m_capacity { N, N }
        { }

    SnapshotRecord(size_t n_nodes, cali::Node **nodes, size_t n, cali_id_t* attr, Variant* data)
        : m_node_array { nodes },
          m_attr_array { attr },
          m_data_array { data },
          m_sizes    { n_nodes, n },
          m_capacity { n_nodes, n }
        { } 

    SnapshotRecord(size_t n, cali_id_t* attr, Variant* data)
        : m_node_array { 0 },
          m_attr_array { attr },
          m_data_array { data },
          m_sizes    { 0, n },
          m_capacity { 0, n }
        { } 

    void append(const SnapshotRecord& list);
    void append(size_t n, const cali_id_t*, const cali::Variant*);
    void append(size_t n, cali::Node* const*, size_t m, const cali_id_t*, const cali::Variant*);
    
    void append(cali::Node* node) {
        if (m_sizes.n_nodes < m_capacity.n_nodes)
            m_node_array[m_sizes.n_nodes++] = node;
    }
    
    void append(cali_id_t attr, const cali::Variant& data) {
        if (m_sizes.n_immediate < m_capacity.n_immediate) {
            m_attr_array[m_sizes.n_immediate] = attr;
            m_data_array[m_sizes.n_immediate] = data;
            ++m_sizes.n_immediate;
        }
    }

    void append(const cali::Attribute& attr, const cali::Variant& data) {
        append(attr.id(), data);
    }

    Sizes capacity() const { 
        return { m_capacity.n_nodes     - m_sizes.n_nodes,
                 m_capacity.n_immediate - m_sizes.n_immediate };
    }

    Sizes size() const {
        return m_sizes;
    }
    
    Data data() const {
        Data addr = { m_node_array, m_attr_array, m_data_array };
        return addr;
    }    

    std::size_t num_nodes() const     { return m_sizes.n_nodes;     }
    std::size_t num_immediate() const { return m_sizes.n_immediate; }
    
    Entry get(const Attribute&) const;

    std::vector<Entry> 
    to_entrylist() const;

private:
    
    cali::Node**   m_node_array;
    cali_id_t*     m_attr_array;
    cali::Variant* m_data_array;
    
    Sizes          m_sizes;
    Sizes          m_capacity;

};

}
