// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
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

/// \file SnapshotRecord.h
/// \brief Snapshot record representation.

#pragma once

#include "common/Attribute.h"
#include "common/ContextRecord.h"
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
    void append(cali::Node*);
    void append(size_t n, const cali_id_t*, const cali::Variant*);
    void append(size_t n, cali::Node* const*, size_t m, const cali_id_t*, const cali::Variant*);
    
    void append(cali_id_t attr, const cali::Variant& data) {
        append(1, &attr, &data);
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

    std::map< cali::Attribute, std::vector<cali::Variant> > 
    unpack(CaliperMetadataAccessInterface&) const;

private:
    
    cali::Node**   m_node_array;
    cali_id_t*     m_attr_array;
    cali::Variant* m_data_array;
    
    Sizes          m_sizes;
    Sizes          m_capacity;

};

}
