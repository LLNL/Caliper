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

///@ file ContextBuffer.cpp
///@ ContextBuffer class implementation

#include "ContextBuffer.h"

#include "Snapshot.h"

#include <Attribute.h>
#include <ContextRecord.h>
#include <Node.h>

#include <util/spinlock.hpp>

#include <algorithm>
#include <cassert>
#include <mutex>
#include <vector>

using namespace cali;
using namespace std;


struct ContextBuffer::ContextBufferImpl
{
    // --- data

    mutable util::spinlock m_lock;

    // m_attr array stores attribute ids for context nodes, hidden entries, and immediate entries
    // m_data array stores context node ids, hidden values, and immediate data
    // boundaries within the arrays are defined by m_num_nodes and m_num_hidden
    // attr/data array layout: [ <node attr/ids> ... <hidden attr/values> ... <data attr/values> ]
    // m_nodes array stores pointers of context nodes

    vector<cali_id_t> m_keys;

    vector<Variant>   m_attr;
    vector<Variant>   m_data;

    vector<Node*>     m_nodes;

    vector<Variant>::size_type m_num_nodes;
    vector<Variant>::size_type m_num_hidden;

    // --- constructor

    ContextBufferImpl() 
        : m_num_nodes { 0 },
          m_num_hidden  { 0 } 
        {
            m_keys.reserve(64);
            m_attr.reserve(64);
            m_data.reserve(64);

            m_nodes.reserve(32);
        }

    // --- interface

    Variant get(const Attribute& attr) const {
        Variant ret;

        std::lock_guard<util::spinlock> lock(m_lock);

        auto it = std::find(m_keys.begin(), m_keys.end(), attr.id());

        if (it != m_keys.end())
            ret = m_data[it-m_keys.begin()];

        return ret;
    }

    Node* get_node(const Attribute& attr) const {
        Node* ret = nullptr;

        std::lock_guard<util::spinlock> lock(m_lock);

        auto end = m_keys.begin() + m_num_nodes;
        auto it  = std::find(m_keys.begin(), end, attr.id());

        if (it != end) {
            vector<Variant>::size_type n = it - m_keys.begin();

            assert(n < m_nodes.size());
            ret = m_nodes[n];
        }

        return ret;
    }

    Variant exchange(const Attribute& attr, const Variant& value) {
        Variant ret;

        {
            std::lock_guard<util::spinlock> lock(m_lock);

            // Only handle immediate or hidden entries for now
            auto it = std::find(m_keys.begin() + m_num_nodes, m_keys.end(), attr.id());

            if (it != m_keys.end()) {
                ret = m_data[it-m_keys.begin()];
                m_data[it-m_keys.begin()] = value;
            }
        }
        
        if (ret.empty())
            set(attr, value);

        return ret;
    }

    cali_err set(const Attribute& attr, const Variant& value) {
        std::lock_guard<util::spinlock> lock(m_lock);

        auto it = std::find(m_keys.begin(), m_keys.end(), attr.id());

        if (it != m_keys.end()) {
            // Update entry

            m_data[it - m_keys.begin()] = value;
        } else {
            // Add new entry

            m_keys.push_back(attr.id());
            m_attr.push_back(Variant(attr.id()));
            m_data.push_back(value);

            if (!attr.store_as_value()) {
                // this is a node, move it up front

                m_nodes.push_back(nullptr);

                if (m_num_nodes < m_keys.size()-1) {
                    std::swap(m_keys.back(), m_keys[m_num_nodes]);
                    std::swap(m_attr.back(), m_attr[m_num_nodes]);
                    std::swap(m_data.back(), m_data[m_num_nodes]);
                }

                ++m_num_nodes;
            } else if (attr.is_hidden()) {
                // move "hidden" entry to the middle
                
                auto n = m_num_nodes + m_num_hidden;

                if (n < m_keys.size()-1) {
                    std::swap(m_keys.back(), m_keys[n]);
                    std::swap(m_attr.back(), m_attr[n]);
                    std::swap(m_data.back(), m_data[n]);
                }

                ++m_num_hidden;
            }
        }

        return CALI_SUCCESS;
    }

    cali_err set_node(const Attribute& attr, Node* node) {
        if (!node || attr.store_as_value())
            return CALI_EINV;

        std::lock_guard<util::spinlock> lock(m_lock);

        auto end = m_keys.begin() + m_num_nodes;
        auto it  = std::find(m_keys.begin(), end, attr.id());

        if (it != end) {
            // Update entry

            vector<Variant>::size_type n = it - m_keys.begin();

            assert(n < m_nodes.size());

            m_data[n]  = Variant(node->id());
            m_nodes[n] = node;
        } else {
            // Add new entry

            m_keys.emplace_back(attr.id());
            m_attr.emplace_back(Variant(attr.id()));
            m_data.emplace_back(node->id());

            m_nodes.push_back(node);

            // this is a node, move entry in attr/data array up front

            if (m_num_nodes < m_keys.size()-1) {
                std::swap(m_keys.back(), m_keys[m_num_nodes]);
                std::swap(m_attr.back(), m_attr[m_num_nodes]);
                std::swap(m_data.back(), m_data[m_num_nodes]);
            }
            if (m_num_hidden > 0) {
                std::swap(m_keys.back(), m_keys[m_num_nodes+m_num_hidden]);
                std::swap(m_attr.back(), m_attr[m_num_nodes+m_num_hidden]);
                std::swap(m_data.back(), m_data[m_num_nodes+m_num_hidden]);
            }

            ++m_num_nodes;
        }

        return CALI_SUCCESS;
    }

    cali_err unset(const Attribute& attr) {
        cali_err ret = CALI_SUCCESS;

        std::lock_guard<util::spinlock> lock(m_lock);

        auto it = std::find(m_keys.begin(), m_keys.end(), attr.id());

        if (it != m_keys.end()) {
            vector<Variant>::size_type n = it - m_keys.begin();

            m_keys.erase(it);
            m_attr.erase(m_attr.begin() + n);
            m_data.erase(m_data.begin() + n);

            if (n < m_nodes.size())
                m_nodes.erase(m_nodes.begin() + n);

            if (n < m_num_nodes)
                --m_num_nodes;
            else if (n < m_num_nodes + m_num_hidden)
                --m_num_hidden;
        }

        return ret;
    }

    void snapshot(Snapshot* sbuf) const {
        Snapshot::Sizes     sizes = sbuf->capacity();
        Snapshot::Addresses addr  = sbuf->addresses();

        std::lock_guard<util::spinlock> lock(m_lock);

        // Copy nodes entries
        int m = std::min(sizes.n_nodes, static_cast<int>(m_nodes.size()));

        std::copy_n(m_nodes.begin(), m, addr.node_entries);
        sizes.n_nodes = m;

        // Copy immediate entries
        std::vector<Variant>::size_type n = m_num_nodes + m_num_hidden;
        m = std::min(sizes.n_data, static_cast<int>(m_data.size()-n));

        std::copy_n(m_keys.begin()+n, m, addr.immediate_attr);
        std::copy_n(m_data.begin()+n, m, addr.immediate_data);

        sizes.n_attr = m;
        sizes.n_data = m;

        sbuf->commit(sizes);
    }

    void push_record(WriteRecordFn fn) {
        std::lock_guard<util::spinlock> lock(m_lock);

        int               n[3] = { static_cast<int>(m_num_nodes), 
                                   static_cast<int>(m_attr.size()-m_num_hidden-m_num_nodes),
                                   static_cast<int>(m_data.size()-m_num_hidden-m_num_nodes) };
        const Variant* data[3] = { m_data.data(), 
                                   m_attr.data() + m_num_nodes + m_num_hidden, 
                                   m_data.data() + m_num_nodes + m_num_hidden };

        fn(ContextRecord::record_descriptor(), n, data);
    }
};


//
// --- ContextBuffer public interface
//

ContextBuffer::ContextBuffer()
    : mP(new ContextBufferImpl)
{ }

ContextBuffer::~ContextBuffer()
{ 
    mP.reset();
}

Variant ContextBuffer::get(const Attribute& attr) const
{
    return mP->get(attr);
}

Node* ContextBuffer::get_node(const Attribute& attr) const
{
    return mP->get_node(attr);
}

Variant ContextBuffer::exchange(const Attribute& attr, const Variant& data)
{
    return mP->exchange(attr, data);
}

cali_err ContextBuffer::set_node(const Attribute& attr, Node* node)
{
    return mP->set_node(attr, node);
}

cali_err ContextBuffer::set(const Attribute& attr, const Variant& data)
{
    return mP->set(attr, data);
}

cali_err ContextBuffer::unset(const Attribute& attr)
{
    return mP->unset(attr);
}

void ContextBuffer::snapshot(Snapshot* sbuf) const
{
    mP->snapshot(sbuf);
}

void ContextBuffer::push_record(WriteRecordFn fn) const
{
    mP->push_record(fn);
}

