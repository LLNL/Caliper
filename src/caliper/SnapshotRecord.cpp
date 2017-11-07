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
SnapshotRecord::append(Node* node)
{
    if (m_sizes.n_nodes >= m_capacity.n_nodes)
        return;

    m_node_array[m_sizes.n_nodes++] = node;
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
        for (int i = 0; i < m_sizes.n_immediate; ++i)
            if (m_attr_array[i] == attr.id())
                return Entry(attr, m_data_array[i]);
    } else {
        for (int i = 0; i < m_sizes.n_nodes; ++i)
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

std::map< Attribute, std::vector<Variant> >
SnapshotRecord::unpack(CaliperMetadataAccessInterface& db) const
{
    std::map< Attribute, std::vector<Variant> > rec;

    // unpack node entries 
    for (int i = 0; i < m_sizes.n_nodes; ++i)
        for (const cali::Node* node = m_node_array[i]; node; node = node->parent())
            if (node->attribute() != CALI_INV_ID)
                rec[db.get_attribute(node->attribute())].push_back(node->data());

    // unpack immediate entries
    for (int i = 0; i < m_sizes.n_immediate; ++i)
        rec[db.get_attribute(m_attr_array[i])].push_back(m_data_array[i]);

    return rec;
}
