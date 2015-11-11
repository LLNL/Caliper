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

///@ file Snapshot.h
///@ Context snapshot definition

#ifndef CALI_SNAPSHOT_H
#define CALI_SNAPSHOT_H

#include <ContextRecord.h>
#include <Node.h>

#include <algorithm>

namespace cali
{

// Snapshots are fixed-size, stack-allocated objects that can be used in 
// a signal handler  

template<int N>
class FixedSnapshot 
{
    Node*     m_nodes[N];
    cali_id_t m_attr[N];
    Variant   m_data[N];

    int       m_num_nodes;
    int       m_num_immediate;

public:

    struct Addresses {
        Node**     node_entries;
        cali_id_t* immediate_attr;
        Variant*   immediate_data;
    };

    struct Sizes {
        int n_nodes;
        int n_attr;
        int n_data;
    };

    FixedSnapshot()
        : m_num_nodes { 0 }, m_num_immediate { 0 }
        { 
            std::fill_n(m_nodes, N, nullptr);
            std::fill_n(m_attr,  N, CALI_INV_ID);
            std::fill_n(m_data,  N, Variant());
        }

    Sizes capacity() const { 
        return { N - m_num_nodes, N-m_num_immediate, N-m_num_immediate }; 
    }

    Addresses addresses() { 
        Addresses addr = { m_nodes+m_num_nodes, m_attr+m_num_immediate, m_data+m_num_immediate };
        return addr;
    }

    void commit(const Sizes& sizes) {
        m_num_nodes     += sizes.n_nodes;
        m_num_immediate += sizes.n_attr;
    }

    void push_record(WriteRecordFn fn) const {
        Variant attr[N];
        Variant node[N];

        for (int i = 0; i < m_num_nodes; ++i)
            node[i] = Variant(m_nodes[i]->id());
        for (int i = 0; i < m_num_immediate; ++i)
            attr[i] = Variant(m_attr[i]);

        int               n[3] = { m_num_nodes, m_num_immediate, m_num_immediate };
        const Variant* data[3] = { node, attr, m_data }; 

        fn(ContextRecord::record_descriptor(), n, data);
    }
};

}

#endif
