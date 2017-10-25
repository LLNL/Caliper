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

/// \file Aggregator.cpp
/// Aggregator implementation

#include "caliper/reader/Aggregator.h"

#include "caliper/reader/QuerySpec.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Log.h"
#include "caliper/common/Node.h"

#include "caliper/common/cali_types.h"

#include "caliper/common/c-util/vlenc.h"

#include "caliper/common/util/split.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <iterator>
#include <mutex>

using namespace cali;
using namespace std;

#define MAX_KEYLEN 32

namespace
{

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
                m_attr = db.create_attribute("count", CALI_TYPE_UINT, CALI_ATTR_ASVALUE);

            return m_attr;
        }        
        
        AggregateKernel* make_kernel() {
            return new CountKernel(this);
        }

        Config()
            : m_attr { Attribute::invalid }
            { }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config;
        }
    };

    CountKernel(Config* config)
        : m_count(0), m_config(config)
        { }
    
    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        cali_id_t count_attr_id = m_config->attribute(db).id();

        for (const Entry& e : list)
            if (e.attribute() == count_attr_id) {
                m_count += e.value().to_uint();
                return;
            }

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

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg.front());
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

    struct StatisticsAttributes {
        Attribute min;
        Attribute max;
        Attribute avg;
        Attribute sum;
        Attribute count;
    };

    class Config : public AggregateKernelConfig {
        std::string          m_target_attr_name;
        Attribute            m_target_attr;

        StatisticsAttributes m_stat_attrs;
        
    public:

        Attribute get_target_attr(CaliperMetadataAccessInterface& db) {
            if (m_target_attr == Attribute::invalid)
                m_target_attr = db.get_attribute(m_target_attr_name);

            return m_target_attr;
        }

        bool get_statistics_attributes(CaliperMetadataAccessInterface& db, StatisticsAttributes& a) {
            if (m_target_attr == Attribute::invalid)
                return false;
            if (a.min != Attribute::invalid) {
                a = m_stat_attrs;
                return true;
            }

            cali_attr_type type = m_target_attr.type();
            int            prop = CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE;

            m_stat_attrs.min = 
                db.create_attribute("min#" + m_target_attr_name, type, prop);
            m_stat_attrs.max = 
                db.create_attribute("max#" + m_target_attr_name, type, prop);
            m_stat_attrs.sum = 
                db.create_attribute("sum#" + m_target_attr_name, type, prop);
            m_stat_attrs.avg = 
                db.create_attribute("avg#" + m_target_attr_name,   CALI_TYPE_DOUBLE, prop);
            m_stat_attrs.count = 
                db.create_attribute("count#" + m_target_attr_name, CALI_TYPE_UINT,   prop);

            a = m_stat_attrs;
            return true;
        }
                
        AggregateKernel* make_kernel() {
            return new StatisticsKernel(this);
        }

        Config(const std::string& name)
            : m_target_attr_name(name),
              m_target_attr(Attribute::invalid)
            {
                Log(2).stream() << "aggregate: creating statistics kernel for attribute " << m_target_attr_name << std::endl;
            }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg.front());
        }        
    };

    StatisticsKernel(Config* config)
        : m_count(0), m_sum(0), m_config(config)
        { }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_lock);

        Attribute target_attr = m_config->get_target_attr(db);
        StatisticsAttributes stat_attr;

        if (!m_config->get_statistics_attributes(db, stat_attr))
            return;
            
        switch (target_attr.type()) {
        case CALI_TYPE_DOUBLE:
        {
            if (m_min.empty())
                m_min = Variant(std::numeric_limits<double>::max());
            if (m_max.empty())
                m_max = Variant(std::numeric_limits<double>::min());

            for (const Entry& e : list) {
                if (e.attribute() == target_attr.id()) {                        
                    double val = e.value().to_double();                        
                    
                    m_sum = Variant(m_sum.to_double() + val);
                    m_min = Variant(std::min(m_min.to_double(), val));
                    m_max = Variant(std::max(m_max.to_double(), val));

                    ++m_count;
                } else if (e.attribute() == stat_attr.min.id()) {
                    m_min = Variant(std::min(e.value().to_double(), m_min.to_double()));
                } else if (e.attribute() == stat_attr.max.id()) {
                    m_max = Variant(std::max(e.value().to_double(), m_max.to_double()));
                } else if (e.attribute() == stat_attr.sum.id()) {
                    m_sum = Variant(e.value().to_double() + m_sum.to_double());
                } else if (e.attribute() == stat_attr.count.id()) {
                    m_count += e.value().to_uint();
                }
            }
        }
        break;
        case CALI_TYPE_INT:
        {
            if (m_min.empty())
                m_min = Variant(std::numeric_limits<int>::max());
            if (m_max.empty())
                m_max = Variant(std::numeric_limits<int>::min());

            for (const Entry& e : list) {
                if (e.attribute() == target_attr.id()) {                        
                    int val = e.value().to_int();                        
                    
                    m_sum = Variant(m_sum.to_int() + val);
                    m_min = Variant(std::min(m_min.to_int(), val));
                    m_max = Variant(std::max(m_max.to_int(), val));

                    ++m_count;
                } else if (e.attribute() == stat_attr.min.id()) {
                    m_min = Variant(std::min(e.value().to_int(), m_min.to_int()));
                } else if (e.attribute() == stat_attr.max.id()) {
                    m_max = Variant(std::max(e.value().to_int(), m_max.to_int()));
                } else if (e.attribute() == stat_attr.sum.id()) {
                    m_sum = Variant(e.value().to_int() + m_sum.to_int());
                } else if (e.attribute() == stat_attr.count.id()) {
                    m_count += e.value().to_uint();
                }
            }
        }
        break;
        case CALI_TYPE_UINT:
        {
            if (m_min.empty())
                m_min = Variant(std::numeric_limits<uint64_t>::max());
            if (m_max.empty())
                m_max = Variant(std::numeric_limits<uint64_t>::min());

            for (const Entry& e : list) {
                if (e.attribute() == target_attr.id()) {                        
                    uint64_t val = e.value().to_uint();                        
                    
                    m_sum = Variant(m_sum.to_uint() + val);
                    m_min = Variant(std::min(m_min.to_uint(), val));
                    m_max = Variant(std::max(m_max.to_uint(), val));

                    ++m_count;
                } else if (e.attribute() == stat_attr.min.id()) {
                    m_min = Variant(std::min(e.value().to_uint(), m_min.to_uint()));
                } else if (e.attribute() == stat_attr.max.id()) {
                    m_max = Variant(std::max(e.value().to_uint(), m_max.to_uint()));
                } else if (e.attribute() == stat_attr.sum.id()) {
                    m_sum = Variant(e.value().to_uint() + m_sum.to_uint());
                } else if (e.attribute() == stat_attr.count.id()) {
                    m_count += e.value().to_uint();
                }
            }
        }
        break;
        default:
            // some error?
            ;
        }
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        if (m_count > 0) {
            StatisticsAttributes stat_attr;

            if (!m_config->get_statistics_attributes(db, stat_attr))
                return;

            list.push_back(Entry(stat_attr.avg, Variant(m_sum.to_double() / m_count)));
            list.push_back(Entry(stat_attr.sum, m_sum));
            list.push_back(Entry(stat_attr.min, m_min));
            list.push_back(Entry(stat_attr.max, m_max));
            list.push_back(Entry(stat_attr.count, Variant(cali_make_variant_from_uint(m_count))));
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

//
// --- PercentageKernel
//

class PercentageKernel : public AggregateKernel {
public:

    class Config : public AggregateKernelConfig {
        std::string          m_target_attr1_name;
        std::string          m_target_attr2_name;
        Attribute            m_target_attr1;
        Attribute            m_target_attr2;

        Attribute m_percentage_attr;

    public:

        std::pair<Attribute,Attribute> get_target_attrs(CaliperMetadataAccessInterface& db) {
            if (m_target_attr1 == Attribute::invalid)
                m_target_attr1 = db.get_attribute(m_target_attr1_name);

            if (m_target_attr2 == Attribute::invalid)
                m_target_attr2 = db.get_attribute(m_target_attr2_name);

            return std::pair<Attribute,Attribute>(m_target_attr1, m_target_attr2);
        }

        bool get_percentage_attribute(CaliperMetadataAccessInterface& db, Attribute& a) {
            if (m_target_attr1 == Attribute::invalid)
                return false;
            if (m_target_attr2 == Attribute::invalid)
                return false;
            if (m_percentage_attr != Attribute::invalid) {
                a = m_percentage_attr;
                return true;
            }

            int            prop = CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE;

            m_percentage_attr = 
                db.create_attribute(m_target_attr1_name + "/" + m_target_attr2_name, CALI_TYPE_DOUBLE, prop);

            a = m_percentage_attr;
            return true;
        }
                
        AggregateKernel* make_kernel() {
            return new PercentageKernel(this);
        }

        Config(const std::vector<std::string>& names)
            : m_target_attr1_name(names.front()), // We have already checked that there are two strings given
              m_target_attr2_name(names.back()),
              m_target_attr1(Attribute::invalid),
              m_target_attr2(Attribute::invalid)
            {
                Log(2).stream() << "aggregate: creating percentage kernel for attributes " 
                                << m_target_attr1_name << " / " << m_target_attr2_name <<std::endl;
            }

        static AggregateKernelConfig* create(const std::vector<std::string>& cfg) {
            return new Config(cfg);
        }        
    };

    PercentageKernel(Config* config)
        : m_sum1(0), m_sum2(0), m_config(config)
        { }

    void increment(Attribute a, Variant& v, const Entry& e) {
        switch (a.type()){
        case CALI_TYPE_DOUBLE:
        {
            double val = e.value().to_double();                        
            v = Variant(v.to_double() + val);
        }
        break;
        case CALI_TYPE_INT:
        {
            int val = e.value().to_int();                        
            v = Variant(v.to_int() + val);
        }
        break;
        case CALI_TYPE_UINT:
        {
            uint64_t val = e.value().to_uint();                        
            v = Variant(v.to_uint() + val);
        }
        break;
        default:
            // some error?
            ;
        }
    }

    virtual void aggregate(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_lock);

        std::pair<Attribute,Attribute> target_attrs = m_config->get_target_attrs(db);
        Attribute percentage_attr = Attribute::invalid;

        if (!m_config->get_percentage_attribute(db, percentage_attr))
            return;

        for (const Entry& e : list) {
            if (e.attribute() == target_attrs.first.id()) {                        
                increment(target_attrs.first, m_sum1, e);
            } else if (e.attribute() == target_attrs.second.id()) {
                increment(target_attrs.second, m_sum2, e);
            } 
        }
    }

    virtual void append_result(CaliperMetadataAccessInterface& db, EntryList& list) {
        if (m_sum2.to_double() > 0) {
            Attribute percentage_attr;

            if (!m_config->get_percentage_attribute(db, percentage_attr))
                return;

            list.push_back(Entry(percentage_attr, Variant(m_sum1.to_double() / m_sum2.to_double())));
        }
    }

private:

    Variant    m_sum1;
    Variant    m_sum2;

    std::mutex m_lock;
    
    Config*    m_config;
};

enum KernelID {
    Count       = 0,
    Sum         = 1,
    Statistics  = 2,
    Percentage  = 3,
};

#define MAX_KERNEL_ID 3

const char* kernel_args[] = { "attribute" };

const QuerySpec::FunctionSignature kernel_signatures[] = {
    { KernelID::Count,      "count",      0, 0, nullptr     },
    { KernelID::Sum,        "sum",        1, 1, kernel_args },
    { KernelID::Statistics, "statistics", 1, 1, kernel_args },
    { KernelID::Percentage, "percentage", 2, 2, kernel_args },
    
    QuerySpec::FunctionSignatureTerminator
};

const struct KernelInfo {
    const char* name;
    AggregateKernelConfig* (*create)(const std::vector<std::string>& cfg);
} kernel_list[] = {
    { "count",      CountKernel::Config::create      },
    { "sum",        SumKernel::Config::create        },
    { "statistics", StatisticsKernel::Config::create },
    { "percentage", PercentageKernel::Config::create },
    { 0, 0 }
};

} // namespace [anonymous]


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
            vector<string> kernelattrs;

            if (cparen != string::npos && cparen > oparen+1) {
                string kernelattrs_str = s.substr(oparen+1, cparen-oparen-1);
                util::split(kernelattrs_str, ',', back_inserter(kernelattrs));
            }

            const ::KernelInfo* ki = ::kernel_list;

            for ( ; ki->name && ki->create && kernelname != ki->name; ++ki)
                ;

            if (ki->create)
                m_kernel_configs.push_back((*ki->create)(kernelattrs));
            else
                Log(0).stream() << "aggregator: unknown aggregation kernel \"" << kernelname << "\"" << std::endl;
        }
    }

    void configure(const QuerySpec& spec) {
        //
        // --- key config
        //
        
        m_kernel_configs.clear();
        m_key_strings.clear();
        m_select_all = false;
        
        switch (spec.aggregation_key.selection) {
        case QuerySpec::AttributeSelection::Default:
        case QuerySpec::AttributeSelection::All:
            m_select_all  = true;
            break;
        case QuerySpec::AttributeSelection::List:
            m_key_strings = spec.aggregation_key.list;
            break;
        default:
            ; 
        }

        //
        // --- kernel config
        //

        m_kernel_configs.clear();

        switch (spec.aggregation_ops.selection) {
        case QuerySpec::AggregationSelection::Default:
        case QuerySpec::AggregationSelection::All:
            m_kernel_configs.push_back(CountKernel::Config::create(vector<string>()));
            // TODO: pick class.aggregatable attributes
            break;
        case QuerySpec::AggregationSelection::List:
            for (const QuerySpec::AggregationOp& k : spec.aggregation_ops.list) {
                if (k.op.id >= 0 && k.op.id <= MAX_KERNEL_ID) {
                    m_kernel_configs.push_back((*::kernel_list[k.op.id].create)(k.args));
                } else {
                    Log(0).stream() << "aggregator: Error: Unknown aggregation kernel "
                                    << k.op.id << " (" << (k.op.name ? k.op.name : "") << ")"
                                    << std::endl;
                }
            }
            break;
        case QuerySpec::AggregationSelection::None:
            break;
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
        
        // --- Group by attribute, reverse nodes (restores original order) and get/create tree node.
        //       Keeps nested attributes separate.

        auto nonnested_begin = std::partition(nodes.begin(), nodes.end(), [&db](const Node* node) {
                return db.get_attribute(node->attribute()).is_nested(); } );

        std::stable_sort(nonnested_begin, nodes.end(), [](const Node* a, const Node* b) {
                             return a->attribute() < b->attribute(); } );

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

    AggregatorImpl() 
        : m_select_all(false)
    {
        m_trie.reserve(4096);
    }

    AggregatorImpl(const QuerySpec& spec) 
        : m_select_all(false)
    {
        configure(spec);
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

Aggregator::Aggregator(const QuerySpec& spec)
    : mP { new AggregatorImpl(spec) }
{
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
Aggregator::add(CaliperMetadataAccessInterface& db, const EntryList& list)
{
    mP->process(db, list);
}

const QuerySpec::FunctionSignature*
Aggregator::aggregation_defs()
{
    return ::kernel_signatures;
}

std::vector<std::string>
Aggregator::aggregation_attribute_names(const QuerySpec& spec)
{
    std::vector<std::string> ret;

    if (spec.aggregation_ops.selection == QuerySpec::AggregationSelection::Default)
        ret.push_back("count");

    if (spec.aggregation_ops.selection == QuerySpec::AggregationSelection::List) {
        for (const QuerySpec::AggregationOp& op : spec.aggregation_ops.list) {
            switch (op.op.id) {
            case KernelID::Count:
                ret.push_back("count");
                break;
            case KernelID::Sum:
                ret.push_back(op.args[0]);
                break;
            case KernelID::Statistics:
                ret.push_back(std::string("min#") + op.args[0]);
                ret.push_back(std::string("max#") + op.args[0]);
                ret.push_back(std::string("avg#") + op.args[0]);
                break;        
            case KernelID::Percentage:
                ret.push_back(op.args[0] + std::string("/") + op.args[1]);
                break;
            }
        }
    }
    
    return ret;
}
