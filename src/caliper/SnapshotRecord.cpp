// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// SnapshotRecord class definition

#include "caliper/SnapshotRecord.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Node.h"

#include <iostream>

using namespace cali;

void
SnapshotRecord::append(const SnapshotRecord& list)
{
    size_t max_nodes     = std::min(list.m_sizes.n_nodes,     m_capacity.n_nodes-m_sizes.n_nodes);
    size_t max_immediate = std::min(list.m_sizes.n_immediate, m_capacity.n_immediate-m_sizes.n_immediate);
        
    std::copy_n(list.m_node_array, max_nodes,
                m_node_array + m_sizes.n_nodes);
    std::copy_n(list.m_attr_array, max_immediate,
                m_attr_array + m_sizes.n_immediate);
    std::copy_n(list.m_data_array, max_immediate,
                m_data_array + m_sizes.n_immediate);

    m_sizes.n_nodes     += max_nodes;
    m_sizes.n_immediate += max_immediate;
}

void
SnapshotRecord::append(size_t n, const cali_id_t* attr_vec, const Variant* data_vec)
{
    size_t max_immediate = std::min(n, m_capacity.n_immediate-m_sizes.n_immediate);
        
    std::copy_n(attr_vec, max_immediate, m_attr_array + m_sizes.n_immediate);
    std::copy_n(data_vec, max_immediate, m_data_array + m_sizes.n_immediate);

    m_sizes.n_immediate += max_immediate;
}

void
SnapshotRecord::append(size_t n, Node* const* node_vec, size_t m, const cali_id_t* attr_vec, const Variant* data_vec)
{
    size_t max_nodes     = std::min(n, m_capacity.n_nodes-m_sizes.n_nodes);
    size_t max_immediate = std::min(m, m_capacity.n_immediate-m_sizes.n_immediate);
    
    std::copy_n(node_vec, max_nodes,     m_node_array + m_sizes.n_nodes);
    std::copy_n(attr_vec, max_immediate, m_attr_array + m_sizes.n_immediate);
    std::copy_n(data_vec, max_immediate, m_data_array + m_sizes.n_immediate);

    m_sizes.n_nodes     += max_nodes;
    m_sizes.n_immediate += max_immediate;
}

Entry
SnapshotRecord::get(const Attribute& attr) const
{
    if (attr == Attribute::invalid)
        return Entry::empty;

    if (attr.store_as_value()) {
        for (size_t i = 0; i < m_sizes.n_immediate; ++i)
            if (m_attr_array[i] == attr.id())
                return Entry(attr, m_data_array[i]);
    } else {
        for (size_t i = 0; i < m_sizes.n_nodes; ++i)
            for (cali::Node* node = m_node_array[i]; node; node = node->parent())
                if (node->attribute() == attr.id())
                    return Entry(node);
    }

    return Entry::empty;
}

std::vector<Entry> 
SnapshotRecord::to_entrylist() const
{
    std::vector<Entry> vec(m_sizes.n_nodes + m_sizes.n_immediate);

    for (size_t i = 0; i < m_sizes.n_nodes; ++i)
        vec.push_back(Entry(m_node_array[i]));
    for (size_t i = 0; i < m_sizes.n_immediate; ++i)
        vec.push_back(Entry(m_attr_array[i], m_data_array[i]));

    return vec;
}
