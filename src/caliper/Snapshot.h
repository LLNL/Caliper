///@ file Snapshot.h
///@ Context snapshot definition

#ifndef CALI_SNAPSHOT_H
#define CALI_SNAPSHOT_H

#include <Attribute.h>
#include <ContextRecord.h>
#include <Entry.h>
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

    Entry get(const Attribute& attr) const {
        if (attr == Attribute::invalid)
            return Entry::empty;

        if (attr.store_as_value()) {
            for (int i = 0; i < m_num_immediate; ++i)
                if (m_attr[i] == attr.id())
                    return Entry(m_attr[i], m_data[i]);
        } else {
            for (int i = 0; i < m_nodes; ++i)
                for (Node* node = m_nodes[i]; node; node = node->parent())
                    if (node->attribute() == attr.id())
                        return Entry(node);
        }

        return Entry::empty;
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
