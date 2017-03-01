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

/// @file Aggregator.cpp
/// Aggregator implementation

#include "Aggregator.h"

#include "CaliperMetadataAccessInterface.h"

#include <Attribute.h>
#include <Log.h>
#include <Node.h>

#include <cali_types.h>

#include <c-util/vlenc.h>

#include <util/split.hpp>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <iterator>
#include <mutex>

using namespace cali;
using namespace std;

#define MAX_KEYLEN 32

class AggregateKernel {
public:

    virtual ~AggregateKernel()
        { }
    
    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) = 0;
    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) = 0;
};

class AggregateKernelConfig
{
public:

    virtual ~AggregateKernelConfig()
        { }

    virtual AggregateKernel* make_kernel() = 0;
};


//
// --- CountKernel
//

class CountKernel : public AggregateKernel {    
public:

    class Config : public AggregateKernelConfig {
        Attribute m_attr;

    public:

        Attribute attribute(CaliperMetadataAccessInterface& db) {
            if (m_attr == Attribute::invalid)
                m_attr = db.create_attribute("aggregate.count", CALI_TYPE_UINT, CALI_ATTR_ASVALUE);

            return m_attr;
        }        
        
        AggregateKernel* make_kernel() {
            return new CountKernel(this);
        }

        Config()
            : m_attr { Attribute::invalid }
            { }

        static AggregateKernelConfig* create(const std::string&) {
            return new Config;
        }
    };

    CountKernel(Config* config)
        : m_count(0), m_config(config)
        { }
    
    virtual void aggregate(CaliperMetadataAccessInterface&, const EntryList&) {
        ++m_count;
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        uint64_t count = m_count.load();
        
        if (count > 0)
            list.push_back(Entry(m_config->attribute(db),
                                 Variant(CALI_TYPE_UINT, &count, sizeof(uint64_t))));
    }

private:

    std::atomic<uint64_t> m_count;
    Config*  m_config;
};


//
// --- SumKernel
//

class SumKernel : public AggregateKernel {
public:

    class Config : public AggregateKernelConfig {
        std::string m_aggr_attr_name;
        Attribute   m_aggr_attr;
        
    public:

        Attribute get_aggr_attr(CaliperMetadataAccessInterface& db) {
            if (m_aggr_attr == Attribute::invalid)
                m_aggr_attr = db.get_attribute(m_aggr_attr_name);

            return m_aggr_attr;
        }
        
        AggregateKernel* make_kernel() {
            return new SumKernel(this);
        }

        Config(const std::string& name)
            : m_aggr_attr_name(name),
              m_aggr_attr(Attribute::invalid)
            {
                Log(2).stream() << "aggregate: creating sum kernel for attribute " << m_aggr_attr_name << std::endl;
            }

        static AggregateKernelConfig* create(const std::string& cfg) {
            return new Config(cfg);
        }        
    };

    SumKernel(Config* config)
        : m_count(0), m_config(config)
        { }
    
    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_lock);
        
        Attribute aggr_attr = m_config->get_aggr_attr(db);

        if (aggr_attr == Attribute::invalid)
            return;
            
        for (const Entry& e : list) {
            if (e.attribute() == aggr_attr.id()) {
                switch (aggr_attr.type()) {
                case CALI_TYPE_DOUBLE:
                    m_sum = Variant(m_sum.to_double() + e.value().to_double());
                    break;
                case CALI_TYPE_INT:
                    m_sum = Variant(m_sum.to_int()    + e.value().to_int()   );
                    break;
                case CALI_TYPE_UINT:
                    m_sum = Variant(m_sum.to_uint()   + e.value().to_uint()  );
                    break;
                default:
                    ;
                    // Some error?!
                }

                ++m_count;
                
                break;
            }
        }
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        if (m_count > 0)
            list.push_back(Entry(m_config->get_aggr_attr(db), m_sum));
    }

private:

    unsigned   m_count;
    Variant    m_sum;
    std::mutex m_lock;
    Config*    m_config;
};

//
// --- StatisticsKernel
//

class StatisticsKernel : public AggregateKernel {
public:

    class Config : public AggregateKernelConfig {
        std::string m_aggr_attr_name;
        Attribute   m_aggr_attr;

        Attribute   m_avg_attr;
        Attribute   m_min_attr;
        Attribute   m_max_attr;

        
    public:

        Attribute get_aggr_attr(CaliperMetadataAccessInterface& db) {
            if (m_aggr_attr == Attribute::invalid)
                m_aggr_attr = db.get_attribute(m_aggr_attr_name);

            return m_aggr_attr;
        }

        Attribute get_avg_attribute(CaliperMetadataAccessInterface& db) {
            if (m_aggr_attr == Attribute::invalid)
                return Attribute::invalid;
            if (m_avg_attr == Attribute::invalid)
                m_avg_attr =
                    db.create_attribute("aggregate.avg#" + m_aggr_attr_name,
                                        CALI_TYPE_DOUBLE,
                                        CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE);

            return m_avg_attr;
        }

        Attribute get_min_attribute(CaliperMetadataAccessInterface& db) {
            if (m_aggr_attr == Attribute::invalid)
                return Attribute::invalid;            
            if (m_min_attr == Attribute::invalid)
                m_min_attr =
                    db.create_attribute("aggregate.min#" + m_aggr_attr_name,
                                        m_aggr_attr.type(),
                                        CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE);

            return m_min_attr;
        }
        
        Attribute get_max_attribute(CaliperMetadataAccessInterface& db) {
            if (m_aggr_attr == Attribute::invalid)
                return Attribute::invalid;            
            if (m_max_attr == Attribute::invalid)
                m_max_attr =
                    db.create_attribute("aggregate.max#" + m_aggr_attr_name,
                                        m_aggr_attr.type(),
                                        CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE);

            return m_max_attr;
        }
                
        AggregateKernel* make_kernel() {
            return new StatisticsKernel(this);
        }

        Config(const std::string& name)
            : m_aggr_attr_name(name),
              m_aggr_attr(Attribute::invalid),
              m_avg_attr(Attribute::invalid),
              m_min_attr(Attribute::invalid),
              m_max_attr(Attribute::invalid)
            {
                Log(2).stream() << "aggregate: creating sum kernel for attribute " << m_aggr_attr_name << std::endl;
            }

        static AggregateKernelConfig* create(const std::string& cfg) {
            return new Config(cfg);
        }        
    };

    StatisticsKernel(Config* config)
        : m_count(0), m_sum(0), m_config(config)
        { }
    
    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_lock);
        
        Attribute aggr_attr = m_config->get_aggr_attr(db);

        if (aggr_attr == Attribute::invalid)
            return;
            
        for (const Entry& e : list) {
            if (e.attribute() == aggr_attr.id()) {
                switch (aggr_attr.type()) {
                case CALI_TYPE_DOUBLE:
                    {
                        if (m_min.empty())
                            m_min = Variant(std::numeric_limits<double>::max());
                        if (m_max.empty())
                            m_max = Variant(std::numeric_limits<double>::min());
                        
                        double val = e.value().to_double();                        
                    
                        m_sum = Variant(m_sum.to_double() + val);
                        m_min = Variant(std::min(m_min.to_double(), val));
                        m_max = Variant(std::max(m_max.to_double(), val));
                    }
                    break;
                case CALI_TYPE_INT:
                    {
                        if (m_min.empty())
                            m_min = Variant(std::numeric_limits<int>::max());
                        if (m_max.empty())
                            m_max = Variant(std::numeric_limits<int>::min());

                        int val = e.value().to_int();
                    
                        m_sum = Variant(m_sum.to_int() + val);
                        m_min = Variant(std::min(m_min.to_int(), val));
                        m_max = Variant(std::max(m_max.to_int(), val));
                    }
                    break;
                case CALI_TYPE_UINT:
                    {
                        if (m_min.empty())
                            m_min = Variant(std::numeric_limits<uint64_t>::max());
                        if (m_max.empty())
                            m_max = Variant(std::numeric_limits<uint64_t>::min());

                        uint64_t val = e.value().to_uint();
                    
                        m_sum = Variant(m_sum.to_uint() + val);
                        m_min = Variant(std::min(m_min.to_uint(), val));
                        m_max = Variant(std::max(m_max.to_uint(), val));
                    }
                    break;
                default:
                    ;
                    // Some error?!
                }

                ++m_count;
                
                break;
            }
        }
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        if (m_count > 0) {
            list.push_back(Entry(m_config->get_avg_attribute(db),
                                 m_sum.to_double() / m_count));
            list.push_back(Entry(m_config->get_min_attribute(db), m_min));
            list.push_back(Entry(m_config->get_max_attribute(db), m_max));
        }
    }

private:

    unsigned   m_count;
    
    Variant    m_sum;
    Variant    m_min;
    Variant    m_max;

    std::mutex m_lock;
    
    Config*    m_config;
};


const struct KernelInfo {
    const char* name;
    AggregateKernelConfig* (*create)(const std::string& cfg);
} kernel_list[] = {
    { "count",      CountKernel::Config::create      },
    { "sum",        SumKernel::Config::create        },
    { "statistics", StatisticsKernel::Config::create },
    { 0, 0 }
};


struct Aggregator::AggregatorImpl
{
    // --- data

    vector<string>         m_key_strings;
    vector<cali_id_t>      m_key_ids;
    std::mutex             m_key_lock;

    bool                   m_select_all;
    
    vector<AggregateKernelConfig*> m_kernel_configs;
    
    struct TrieNode {
        uint32_t next[256] = { 0 };
        vector<AggregateKernel*> kernels;

        ~TrieNode() {
            for (AggregateKernel* k : kernels)
                delete k;
            kernels.clear();
        }
    };

    std::vector<TrieNode*> m_trie;    
    std::mutex             m_trie_lock;
    
    //
    // --- parse config
    //

    void parse_key(const string& key) {
        util::split(key, ':', back_inserter(m_key_strings));
        m_select_all = m_key_strings.empty();
    }

    void parse_aggr_config(const string& configstr) {
        vector<string> aggregators;
        
        util::split(configstr, ':', back_inserter(aggregators));

        for (const string& s : aggregators) {
            string::size_type oparen = s.find_first_of('(');
            string::size_type cparen = s.find_last_of(')');

            string kernelname = s.substr(0, oparen);
            string kernelconfig;

            if (cparen != string::npos && cparen > oparen+1)
                kernelconfig = s.substr(oparen+1, cparen-oparen-1);

            const KernelInfo* ki = kernel_list;

            for ( ; ki->name && ki->create && kernelname != ki->name; ++ki)
                ;

            if (ki->create)
                m_kernel_configs.push_back((*ki->create)(kernelconfig));
            else
                Log(0).stream() << "aggregator: unknown aggregation kernel \"" << kernelname << "\"" << std::endl;
        }
    }
    
    //
    // --- aggregation db ops
    //
    
    TrieNode* get_trienode(uint32_t id) {
        // Assume trie is locked!!
        
        if (id >= m_trie.size())
            m_trie.resize(id+1);

        if (!m_trie[id])
            m_trie[id] = new TrieNode;

        return m_trie[id];
    }

    TrieNode* find_trienode(size_t n, unsigned char* key) {
        std::lock_guard<std::mutex>
            g(m_trie_lock);

        TrieNode* trie = get_trienode(0);

        for ( size_t i = 0; trie && i < n; ++i ) {
            uint32_t id = trie->next[key[i]];

            if (!id) {
                id = static_cast<uint32_t>(m_trie.size()+1);
                trie->next[key[i]] = id;
            }

            trie = get_trienode(id);
        }

        if (trie->kernels.empty())
            for (AggregateKernelConfig* c : m_kernel_configs)
                trie->kernels.push_back(c->make_kernel());
        
        return trie;
    }

    //
    // --- snapshot processing
    //

    std::vector<cali_id_t> update_key_attribute_ids(CaliperMetadataAccessInterface& db) {
        std::lock_guard<std::mutex>
            g(m_key_lock);
        
        auto it = m_key_strings.begin();
        
        while (it != m_key_strings.end()) {
            Attribute attr = db.get_attribute(*it);

            if (attr != Attribute::invalid) {
                m_key_ids.push_back(attr.id());
                it = m_key_strings.erase(it);
            } else
                ++it;
        }

        return m_key_ids;
    }

    size_t pack_key(const Node* key_node, const vector<Entry>& immediates, CaliperMetadataAccessInterface& db, unsigned char* key) {
        unsigned char node_key[10];
        size_t        node_key_len  = 0;

        if (key_node)
            node_key_len = vlenc_u64(key_node->id(), node_key);

        uint64_t      num_immediate = 0;
        unsigned char imm_key[MAX_KEYLEN];
        size_t        imm_key_len = 0;
        
        for (const Entry& e : immediates) {
            unsigned char buf[32]; // max space for one immediate entry
            size_t        p = 0;

            // need to convert the Variant to its actual type before saving
            bool    ok = true;
            Variant v  = e.value();

            if (!ok)
                continue;
            
            p += vlenc_u64(e.attribute(), buf+p);
            p += v.pack(buf+p);

            // discard entry if it won't fit in the key buf :-(
            if (p + imm_key_len + node_key_len + 1 >= MAX_KEYLEN)
                break;

            ++num_immediate;
            memcpy(imm_key+imm_key_len, buf, p);
            imm_key_len += p;
        }
        
        size_t pos = vlenc_u64(2*num_immediate + (key_node ? 1 : 0), key);
        
        memcpy(key+pos, node_key, node_key_len);
        pos += node_key_len;
        memcpy(key+pos, imm_key,  imm_key_len);
        pos += imm_key_len;

        return pos;
    }
    
    void process(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::vector<cali_id_t>   key_ids = update_key_attribute_ids(db);
                
        // --- Unravel nodes, filter for key attributes

        std::vector<const Node*> nodes;
        std::vector<Entry>       immediates;
        
        nodes.reserve(80);
        immediates.reserve(key_ids.size());        

        bool select_all = m_select_all;
        
        for (const Entry& e : list)
            for (const Node* node = e.node(); node && node->attribute() != CALI_INV_ID; node = node->parent())
                if (select_all || std::find(key_ids.begin(), key_ids.end(), node->attribute()) != key_ids.end())
                    nodes.push_back(node);    

        // Only include explicitly selected immediate entries in the key.
        // Add them in key_ids order to make sure they're normalized
        for (cali_id_t id : key_ids)
            for (const Entry& e : list)
                if (e.is_immediate() && e.attribute() == id)
                    immediates.push_back(e);                
        
        // --- Group by attribute, reverse nodes (restores original order) and get/create tree node

        std::stable_sort(nodes.begin(), nodes.end(),
                    [](const Node* a, const Node* b) { return a->attribute() < b->attribute(); } );

        std::reverse(nodes.begin(), nodes.end());

        const Node*   key_node = db.make_tree_entry(nodes.size(), nodes.data());

        // --- Pack key

        unsigned char key[MAX_KEYLEN];
        size_t        pos  = pack_key(key_node, immediates, db, key);

        TrieNode*     trie = find_trienode(pos, key);

        if (!trie)
            return;

        // --- Aggregate
        
        for (AggregateKernel* k : trie->kernels)
            k->aggregate(db, list);
    }

    //
    // --- Flush
    //

    void unpack_key(const unsigned char* key, const CaliperMetadataAccessInterface& db, EntryList& list) {
        // key format: 2*N + node flag, node id, N * (attr id, Variant)

        size_t   p = 0;
        uint64_t n = vldec_u64(key + p, &p);

        if (n % 2 == 1)
            list.push_back(Entry(db.node(vldec_u64(key + p, &p))));

        for (unsigned i = 0; i < n/2; ++i) {
            bool      ok   = true;
            
            cali_id_t id   = static_cast<cali_id_t>(vldec_u64(key+p, &p));
            Variant   data = Variant::unpack(key+p, &p, &ok);

            if (ok)
                list.push_back(Entry(db.get_attribute(id), data));
        }
    }
    
    void recursive_flush(size_t n, unsigned char* key, TrieNode* trie,
                         CaliperMetadataAccessInterface& db, const SnapshotProcessFn push) {
        if (!trie)
            return;

        // --- Write current entry (if it represents a snapshot)
        
        if (!trie->kernels.empty()) {
            EntryList list;

            // Decode & add key node entry
            unpack_key(key, db, list);

            // Write aggregation variables
            for (AggregateKernel* k : trie->kernels)
                k->append_result(db, list);

            push(db, list);
        }

        // --- Recursively iterate over subsequent DB entries

        unsigned char* next_key = static_cast<unsigned char*>(alloca(n+2));

        memset(next_key, 0, n+2);
        memcpy(next_key, key, n);
        
        for (size_t i = 0; i < 256; ++i) {
            if (trie->next[i] == 0)
                continue;

            TrieNode* next = get_trienode(trie->next[i]);
            next_key[n]    = static_cast<unsigned char>(i);

            recursive_flush(n+1, next_key, next, db, push);
        }
    }
    
    void flush(CaliperMetadataAccessInterface& db, const SnapshotProcessFn push) {
        // NOTE: No locking: we assume flush() runs serially!
        
        TrieNode*     trie = get_trienode(0);
        unsigned char key  = 0;

        recursive_flush(0, &key, trie, db, push);
    }

    AggregatorImpl() {
        m_trie.reserve(4096);
    }
    
    ~AggregatorImpl() {
        for (TrieNode* e : m_trie)
            delete e;

        m_trie.clear();
        
        for (AggregateKernelConfig* c : m_kernel_configs)
            delete c;

        m_kernel_configs.clear();
    }
};

Aggregator::Aggregator(const string& aggr_config, const string& key)
    : mP { new AggregatorImpl() }
{
    mP->parse_aggr_config(aggr_config);
    mP->parse_key(key);
}

Aggregator::~Aggregator()
{
    mP.reset();
}

void 
Aggregator::flush(CaliperMetadataAccessInterface& db, SnapshotProcessFn push)
{
    mP->flush(db, push);
}

void
Aggregator::operator()(CaliperMetadataAccessInterface& db, const EntryList& list)
{
    mP->process(db, list);
}
