///@ file ContextBuffer.cpp
///@ ContextBuffer class implementation

#include "ContextBuffer.h"

#include "SigsafeRWLock.h"

#include <Attribute.h>
#include <ContextRecord.h>
#include <Node.h>

#include <algorithm>
#include <cassert>
#include <vector>

#include <iterator>
#include <iostream>

using namespace cali;
using namespace std;


struct ContextBuffer::ContextBufferImpl
{
    // --- data

    mutable SigsafeRWLock  m_lock;

    // m_attr array stores attribute ids for context nodes, hidden entries, and immediate entries
    // m_data array stores context node ids, hidden values, and immediate data
    // boundaries within the arrays are defined by m_num_nodes and m_num_hidden
    // attr/data array layout: [ <node attr/ids> ... <hidden attr/values> ... <data attr/values> ]
    // m_nodes array stores pointers of context nodes

    vector<Variant> m_attr;
    vector<Variant> m_data;

    vector<Node*>   m_nodes;

    vector<Variant>::size_type m_num_nodes;
    vector<Variant>::size_type m_num_hidden;

    // --- constructor

    ContextBufferImpl() 
        : m_num_nodes { 0 },
        m_num_hidden  { 0 } { 
        m_attr.reserve(64);
        m_data.reserve(64);

        m_nodes.reserve(32);
    }

    // --- interface

    Variant get(const Attribute& attr) const {
        Variant ret;

        m_lock.rlock();

        auto it = std::find(m_attr.begin(), m_attr.end(), Variant(attr.id()));

        if (it != m_attr.end())
            ret = m_data[it-m_attr.begin()];

        m_lock.unlock();

        return ret;
    }

    Node* get_node(const Attribute& attr) const {
        Node* ret = nullptr;

        m_lock.rlock();

        auto it = std::find(m_attr.begin(), m_attr.end(), Variant(attr.id()));

        if (it != m_attr.end()) {
            vector<Variant>::size_type n = it - m_attr.begin();
            assert(n < m_nodes.size());
            ret = m_nodes[n];
        }

        m_lock.unlock();

        return ret;
    }

    cali_err set(const Attribute& attr, const Variant& value) {
        m_lock.wlock();

        auto it = std::find(m_attr.begin(), m_attr.end(), Variant(attr.id()));

        if (it != m_attr.end()) {
            // Update entry

            m_data[it - m_attr.begin()] = value;
        } else {
            // Add new entry

            m_attr.push_back(Variant(attr.id()));
            m_data.push_back(value);

            if (!attr.store_as_value()) {
                // this is a node, move it up front

                m_nodes.push_back(nullptr);

                if (m_num_nodes < m_attr.size()-1) {
                    std::swap(m_attr.back(), m_attr[m_num_nodes]);
                    std::swap(m_data.back(), m_data[m_num_nodes]);
                }

                ++m_num_nodes;
            } else if (attr.is_hidden()) {
                // move "hidden" entry to the middle
                
                auto n = m_num_nodes + m_num_hidden;

                if (n < m_attr.size()-1) {
                    std::swap(m_attr.back(), m_attr[n]);
                    std::swap(m_data.back(), m_data[n]);
                }

                ++m_num_hidden;
            }
        }

        m_lock.unlock();

        return CALI_SUCCESS;
    }

    cali_err set_node(const Attribute& attr, Node* node) {
        if (!node || attr.store_as_value())
            return CALI_EINV;

        m_lock.wlock();

        auto it = std::find(m_attr.begin(), m_attr.end(), Variant(attr.id()));

        if (it != m_attr.end()) {
            // Update entry

            vector<Variant>::size_type n = it - m_attr.begin();

            assert(n < m_nodes.size());

            m_data[n]  = Variant(node->id());
            m_nodes[n] = node;
        } else {
            // Add new entry

            m_attr.emplace_back(attr.id());
            m_data.emplace_back(node->id());

            m_nodes.push_back(node);

            // this is a node, move entry in attr/data array up front

            if (m_num_nodes < m_attr.size()-1) {
                std::swap(m_attr.back(), m_attr[m_num_nodes]);
                std::swap(m_data.back(), m_data[m_num_nodes]);
            }
            if (m_num_hidden > 0) {
                std::swap(m_attr.back(), m_attr[m_num_nodes+m_num_hidden-1]);
                std::swap(m_data.back(), m_data[m_num_nodes+m_num_hidden-1]);
            }

            ++m_num_nodes;
        }

        m_lock.unlock();

        return CALI_SUCCESS;
    }

    cali_err unset(const Attribute& attr) {
        cali_err ret = CALI_SUCCESS;

        m_lock.wlock();

        auto it = std::find(m_attr.begin(), m_attr.end(), Variant(attr.id()));

        if (it != m_attr.end()) {
            vector<Variant>::size_type n = it - m_attr.begin();

            m_attr.erase(it);
            m_data.erase(m_data.begin() + n);

            if (n < m_nodes.size())
                m_nodes.erase(m_nodes.begin() + n);

            if (n < m_num_nodes)
                --m_num_nodes;
            else if (n < m_num_nodes + m_num_hidden)
                --m_num_hidden;
        }

        m_lock.unlock();

        return ret;
    }

    size_t pull_context(uint64_t* buf, size_t size) const {
        unsigned bufidx = 0;

        m_lock.rlock();

        std::vector<Variant>::size_type n = 0;

        for (n = 0; n < m_num_nodes && bufidx < size; ++n)
            buf[bufidx++] = m_attr[n].to_id();
        for (n = m_num_nodes + m_num_hidden; n < m_attr.size() && bufidx < size; ++n) {
            buf[bufidx++] = m_attr[n].to_id();

            uint64_t data = 0;
            memcpy(&data, m_data[n].data(), std::min(sizeof(uint64_t), m_data[n].size()));

            buf[bufidx++] = data;
        }

        m_lock.unlock();

        return bufidx;
    }

    void push_record(WriteRecordFn fn) {
        m_lock.rlock();

        int               n[3] = { static_cast<int>(m_num_nodes), 
                                   static_cast<int>(m_attr.size()-m_num_hidden-m_num_nodes),
                                   static_cast<int>(m_data.size()-m_num_hidden-m_num_nodes) };
        const Variant* data[3] = { m_data.data(), 
                                   m_attr.data() + m_num_nodes + m_num_hidden, 
                                   m_data.data() + m_num_nodes + m_num_hidden };

        fn(ContextRecord::record_descriptor(), n, data);

        m_lock.unlock();
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

size_t ContextBuffer::pull_context(uint64_t* buf, size_t size) const
{
    return mP->pull_context(buf, size);
}

void ContextBuffer::push_record(WriteRecordFn fn) const
{
    mP->push_record(fn);
}

