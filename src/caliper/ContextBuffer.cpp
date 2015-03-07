///@ file ContextBuffer.cpp
///@ ContextBuffer class implementation

#include "ContextBuffer.h"

#include "SigsafeRWLock.h"

#include <Attribute.h>
#include <ContextRecord.h>

#include <cassert>
#include <vector>


using namespace cali;
using namespace std;


struct ContextBuffer::ContextBufferImpl
{
    // --- data

    mutable SigsafeRWLock  m_lock;

    vector<Variant> m_node;
    vector<Variant> m_attr;
    vector<Variant> m_data;

    vector< pair<cali_id_t, int> > m_indices;

    // --- constructor

    ContextBufferImpl() { 
        m_node.reserve(32);
        m_attr.reserve(32);
        m_data.reserve(32);

        m_indices.reserve(64);
    }

    // --- interface

    Variant get(const Attribute& attr) const {
        Variant ret;

        m_lock.rlock();

        auto index_it = lower_bound(m_indices.begin(), m_indices.end(), make_pair(attr.id(), 0));        

        if (index_it != m_indices.end() && index_it->first == attr.id())
            ret = (attr.store_as_value() ? m_data[index_it->second] : m_node[index_it->second]);

        m_lock.unlock();

        return ret;
    }

    cali_err set(const Attribute& attr, const Variant& value) {
        m_lock.wlock();

        auto index_it = lower_bound(m_indices.begin(), m_indices.end(), make_pair(attr.id(), 0));

        if (index_it == m_indices.end() || index_it->first != attr.id()) {
            int index = 0;

            if (attr.store_as_value()) {
                assert(m_attr.size() == m_data.size());

                // add new "explicit" context entry
                index = m_attr.size();
                m_attr.push_back(Variant(attr.id()));
                m_data.push_back(value);
            } else {
                // add new "implicit" context entry ("value" is a node ID)
                index = m_node.size();
                m_node.push_back(value);
            }

            m_indices.insert(index_it, make_pair(attr.id(), index));
        } else {
            if (attr.store_as_value())
                // replace existing "explicit" context entry
                m_data[index_it->second] = value;
            else
                // replace existing "implicit" context (value is a node ID)
                m_node[index_it->second] = value;
        }

        m_lock.unlock();

        return CALI_SUCCESS;
    }

    cali_err unset(const Attribute& attr) {
        cali_err ret = CALI_SUCCESS;

        m_lock.wlock();

        auto index_it = lower_bound(m_indices.begin(), m_indices.end(), make_pair(attr.id(), 0));

        if (index_it != m_indices.end()) {
            if (attr.store_as_value()) {
                m_attr.erase(m_attr.begin() + index_it->second);
                m_data.erase(m_data.begin() + index_it->second);
            } else 
                m_node.erase(m_node.begin() + index_it->second);

            m_indices.erase(index_it);
        } else 
            ret = CALI_EINV;

        m_lock.unlock();

        return ret;
    }

    size_t pull_context(uint64_t* buf, size_t size) const {
        unsigned bufidx = 0;

        m_lock.rlock();

        for (auto it = m_node.begin(); it != m_node.end() && bufidx < size; ++it)
            buf[bufidx++] = it->to_id();
        for (unsigned i = 0; i < m_attr.size() && i < m_data.size() && bufidx < size; ++i) {
            buf[bufidx++] = m_attr[i].to_id();

            uint64_t data = 0;
            memcpy(&data, m_data[i].data(), std::min(sizeof(uint64_t), m_data[i].size()));

            buf[bufidx++] = data;
        }

        m_lock.unlock();

        return bufidx;
    }

    void push_record(WriteRecordFn fn) {
        m_lock.rlock();

        int               n[3] = { m_node.size(), m_attr.size(), m_data.size() };
        const Variant* data[3] = { m_node.data(), m_attr.data(), m_data.data() };

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

