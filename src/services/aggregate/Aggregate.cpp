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

/// \file  Aggregate.cpp
/// \brief Caliper on-line aggregation service

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/ContextRecord.h"
#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/Variant.h"

#include "caliper/common/c-util/unitfmt.h"
#include "caliper/common/c-util/vlenc.h"

#include "caliper/common/util/spinlock.hpp"

#include <pthread.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <mutex>
#include <vector>
#include <unordered_set>

using namespace cali;
using namespace std;

#define MAX_KEYLEN          32
#define SNAP_MAX            80 // max snapshot size

//
// --- Class for the per-thread aggregation database
//

class AggregateDB {
    //
    // --- members
    //

    std::atomic<bool>        m_stopped;
    std::atomic<bool>        m_retired;

    AggregateDB*             m_next;
    AggregateDB*             m_prev;

    // the actual aggregation db

    struct AggregateKernel  {
        double   min;
        double   max;
        double   sum;
		double   avg;
        int      count;

        AggregateKernel()
            : min(std::numeric_limits<double>::max()),
              max(std::numeric_limits<double>::min()),
              sum(0), count(0), avg(0)
        { }

        void add(double val) {
            min  = std::min(min, val);
            max  = std::max(max, val);
            sum += val;
			avg = ((count*avg) + val)/ (count + 1.0);
            ++count;
        }
    };

    struct TrieNode {
        uint32_t next[256] = { 0 };
        uint32_t k_id      = 0xFFFFFFFF;
        uint32_t count     = 0;
    };

    template<typename T, size_t MAX_BLOCKS = 2048, size_t ENTRIES_PER_BLOCK = 1024>
    class BlockAlloc {
        T*     m_blocks[MAX_BLOCKS] = { 0 };
        size_t m_num_blocks;

    public:

        T* get(size_t id, bool alloc) {
            size_t block = id / ENTRIES_PER_BLOCK;

            if (block > MAX_BLOCKS)
                return 0;

            if (!m_blocks[block]) {
                if (alloc) {
                    m_blocks[block] = new T[ENTRIES_PER_BLOCK];
                    ++m_num_blocks;
                } else
                    return 0;
            }

            return m_blocks[block] + (id % ENTRIES_PER_BLOCK);
        }

        void clear() {
            for (size_t b = 0; b < MAX_BLOCKS; ++b)
                delete[] m_blocks[b];
            
            std::fill_n(m_blocks, MAX_BLOCKS, nullptr);
            
            m_num_blocks = 0;
        }

        size_t num_blocks() const {
            return m_num_blocks;
        }

        BlockAlloc() {

        }

        ~BlockAlloc() {
            clear();
        }
    };

    BlockAlloc<TrieNode>        m_trie;
    BlockAlloc<AggregateKernel> m_kernels;

    Node                        m_aggr_root_node;
    
    // we maintain some internal statistics
    size_t                   m_num_trie_entries;
    size_t                   m_num_kernel_entries;
    size_t                   m_num_dropped;
    size_t                   m_num_skipped_keys;
    size_t                   m_max_keylen;

    //
    // --- static data
    //

    struct StatisticsAttributes {
        Attribute min_attr;
        Attribute max_attr;
        Attribute sum_attr;
        Attribute avg_attr;
    };

    static Attribute         s_count_attribute;

    static vector<cali_id_t> s_key_attribute_ids;
    static vector<Attribute> s_key_attributes;
    static vector<string>    s_key_attribute_names;
    static vector<Attribute> s_aggr_attributes;
    static vector<string>    s_aggr_attribute_names;
    static vector<StatisticsAttributes>
                             s_stats_attributes;

    static const ConfigSet::Entry
                             s_configdata[];
    static ConfigSet         s_config;

    static pthread_key_t     s_aggregate_db_key;

    static AggregateDB*      s_list;
    static util::spinlock    s_list_lock;

    // global statistics
    static size_t            s_global_num_trie_entries;
    static size_t            s_global_num_kernel_entries;
    static size_t            s_global_num_trie_blocks;
    static size_t            s_global_num_kernel_blocks;
    static size_t            s_global_num_dropped;
    static size_t            s_global_num_skipped_keys;
    static size_t            s_global_max_keylen;


    //
    // --- helper functions
    //

    void unlink() {
        if (m_next)
            m_next->m_prev = m_prev;
        if (m_prev)
            m_prev->m_next = m_next;
    }

    TrieNode* find_entry(size_t n, unsigned char* key, bool alloc) {
        TrieNode* entry = m_trie.get(0, alloc);

        for ( size_t i = 0; entry && i < n; ++i ) {
            uint32_t id = entry->next[key[i]];

            if (!id) {
                id = static_cast<uint32_t>(++m_num_trie_entries);
                entry->next[key[i]] = id;
            }

            entry = m_trie.get(id, alloc);
        }
        
        if (entry && entry->k_id == 0xFFFFFFFF) {
            size_t num_ids = s_aggr_attributes.size();

            if (num_ids > 0) {
                uint32_t first_id = static_cast<uint32_t>(m_num_kernel_entries + 1);

                m_num_kernel_entries += num_ids;

                for (unsigned i = 0; i < num_ids; ++i)
                    if (m_kernels.get(first_id + i, alloc) == 0)
                        return 0;

                entry->k_id = first_id;
            }
        }

        return entry;
    }

    void write_aggregated_snapshot(const unsigned char* key, const TrieNode* entry, Caliper* c,
                                   Caliper::SnapshotFlushFn proc_fn) {
        SnapshotRecord::FixedSnapshotRecord<SNAP_MAX> snapshot_data;
        SnapshotRecord snapshot(snapshot_data);

        // --- decode key

        size_t    p = 0;

        uint64_t  toc = vldec_u64(key+p, &p); // first entry is 2*num_nodes + (1 : w/ immediate, 0 : w/o immediate)
        int       num_nodes = static_cast<int>(toc)/2;
        
        for (int i = 0; i < std::min(num_nodes, SNAP_MAX); ++i)
            snapshot.append(c->node(vldec_u64(key + p, &p)));

        if (toc % 2 == 1) {
            // there are immediate key entries

            uint64_t imm_bitfield = vldec_u64(key+p, &p);

            for (size_t k = 0; k < s_key_attribute_ids.size(); ++k)
                if (imm_bitfield & (1 << k)) {
                    uint64_t val = vldec_u64(key+p, &p);
                    Variant  v(s_key_attributes[k].type(), &val, sizeof(uint64_t));

                    snapshot.append(s_key_attribute_ids[k], v);
                }
        }

        // --- write aggregate entries

        int       num_aggr_attr = s_aggr_attributes.size();

        Variant   attr_vec[SNAP_MAX];
        Variant   data_vec[SNAP_MAX];

        for (int a = 0; a < std::min(num_aggr_attr, SNAP_MAX/3); ++a) {
            AggregateKernel* k = m_kernels.get(entry->k_id+a, false);

            if (!k)
                break;
            if (k->count == 0)
                continue;

            snapshot.append(s_stats_attributes[a].min_attr.id(), Variant(k->min));
            snapshot.append(s_stats_attributes[a].max_attr.id(), Variant(k->max));
            snapshot.append(s_stats_attributes[a].sum_attr.id(), Variant(k->sum));
            snapshot.append(s_stats_attributes[a].avg_attr.id(), Variant(k->avg));
        }

        uint64_t count = entry->count;

        snapshot.append(s_count_attribute.id(), Variant(CALI_TYPE_UINT, &count, sizeof(uint64_t)));

        // --- write snapshot record

        proc_fn(&snapshot);
    }

    size_t recursive_flush(size_t n, unsigned char* key, TrieNode* entry, Caliper* c, Caliper::SnapshotFlushFn proc_fn) {
        if (!entry)
            return 0;

        size_t num_written = 0;

        // --- write current entry if it represents a snapshot

        if (entry->count > 0)
            write_aggregated_snapshot(key, entry, c, proc_fn);

        num_written += (entry->count > 0 ? 1 : 0);

        // --- iterate over sub-records

        unsigned char* next_key = static_cast<unsigned char*>(alloca(n+2));

        memset(next_key, 0, n+2);
        memcpy(next_key, key, n);

        for (size_t i = 0; i < 256; ++i) {
            if (entry->next[i] == 0)
                continue;

            TrieNode* e  = m_trie.get(entry->next[i], false);
            next_key[n]  = static_cast<unsigned char>(i);

            num_written += recursive_flush(n+1, next_key, e, c, proc_fn);
        }

        return num_written;
    }

    static void init_aggregation_attributes(Caliper* c, const std::vector<std::string>& aggr_attr_names) {
        // Init aggregation attributes

        if (aggr_attr_names.empty()) {
            // find all attributes of class "class.aggregatable"

            s_aggr_attributes = 
                c->find_attributes_with(c->get_attribute("class.aggregatable"));
        } else if (aggr_attr_names.front() != "none") {
            for (const std::string& name : aggr_attr_names) {
                Attribute attr = c->get_attribute(name);

                if (attr == Attribute::invalid) {
                    Log(1).stream() << "Aggregate: Warning: Aggregation attribute \"" 
                                    << name
                                    << "\" not found." 
                                    << std::endl;

                    continue;
                }

                cali_attr_type type = attr.type();

                if (!(type == CALI_TYPE_INT  || 
                      type == CALI_TYPE_UINT || 
                      type == CALI_TYPE_DOUBLE)) {
                    Log(1).stream() << "Aggregate: Warning: Aggregation attribute \""
                                    << name << "\" has invalid type \"" 
                                    << cali_type2string(type) << "\""
                                    << std::endl;

                    continue;
                }

                s_aggr_attributes.push_back(attr);
            }
        }

        // Create the derived statistics attributes

        s_stats_attributes.resize(s_aggr_attributes.size());

        for (size_t i = 0; i < s_aggr_attributes.size(); ++i) {
            std::string name = s_aggr_attributes[i].name();

            s_stats_attributes[i].min_attr =
                c->create_attribute(std::string("min#") + name,
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);
            s_stats_attributes[i].max_attr =
                c->create_attribute(std::string("max#") + name,
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);
            s_stats_attributes[i].sum_attr =
                c->create_attribute(std::string("sum#") + name,
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);
            
			s_stats_attributes[i].avg_attr =
                c->create_attribute(std::string("avg#") + name,
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);
        }

        s_count_attribute =
            c->create_attribute("count",
                                CALI_TYPE_INT, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);
    }

    static bool init_static_data() {
        s_list_lock.unlock();

        s_config = RuntimeConfig::init("aggregate", s_configdata);

        s_key_attribute_names =
            s_config.get("key").to_stringlist(",:");

        s_key_attribute_ids.assign(s_key_attribute_names.size(), CALI_INV_ID);
        s_key_attributes.assign(s_key_attribute_names.size(), Attribute::invalid);
        
        if (pthread_key_create(&s_aggregate_db_key, retire) != 0) {
            Log(0).stream() << "aggregate: error: pthread_key_create() failed"
                            << std::endl;
            return false;
        }

        return true;
    }

public:

    void clear() {
        m_trie.clear();
        m_kernels.clear();

        m_num_trie_entries   = 0;
        m_num_kernel_entries = 0;
        m_num_dropped        = 0;
        m_num_skipped_keys   = 0;
        m_max_keylen         = 0;
    }

    void process_snapshot(Caliper* c, const SnapshotRecord* snapshot) {
        SnapshotRecord::Sizes sizes = snapshot->size();

        if (sizes.n_nodes + sizes.n_immediate == 0)
            return;

        SnapshotRecord::Data addr   = snapshot->data();

        //
        // --- create / get context tree nodes for key
        //
        
        cali_id_t   key_node   = CALI_INV_ID;
        cali_id_t*  nodeid_vec = &key_node;
        uint64_t    n_nodes    = 0;

        size_t      n_key_attr = 0;
        cali_id_t*  key_attribute_ids = static_cast<cali_id_t*>(alloca(s_key_attribute_ids.size() * sizeof(cali_id_t)));

        // create list of all valid key attribute ids
        for (size_t i = 0; i < s_key_attribute_ids.size(); ++i)
            if (s_key_attribute_ids[i] != CALI_INV_ID)
                key_attribute_ids[n_key_attr++] = s_key_attribute_ids[i];
            
        if (n_key_attr > 0 && sizes.n_nodes > 0) {
            // --- find out number of key node entries
            
            size_t       key_entries = 0;
            const Node* *start_nodes = static_cast<const Node**>(alloca(sizes.n_nodes * sizeof(const Node*)));

            memset(start_nodes, 0, sizes.n_nodes * sizeof(const Node*));
            
            for (size_t i = 0; i < sizes.n_nodes; ++i)
                for (const Node* node = addr.node_entries[i]; node; node = node->parent())
                    for (size_t a = 0; a < n_key_attr; ++a)
                        if (key_attribute_ids[a] == node->attribute()) {
                            ++key_entries;

                            // Save the snapshot nodes that lead to key nodes
                            // to short-cut the subsequent loop a bit 
                            if (!start_nodes[i])
                                start_nodes[i] = node;
                        }

            // FIXME: Strictly, we need to partition between nested/nonnested attribute nodes here
            //   and order nonnested ones by attribute ID, as nodes of different attributes 
            //   can appear in any order in the context tree.

            // --- construct path of key nodes in reverse order, make/find new entry

            if (key_entries > 0) {
                const Node* *nodelist = static_cast<const Node**>(alloca(key_entries*sizeof(const Node*)));
                size_t       filled   = 0;

                memset(nodelist, 0, key_entries * sizeof(const Node*));
                
                for (size_t i = 0; i < sizes.n_nodes; ++i)
                    for (const Node* node = start_nodes[i]; node; node = node->parent())
                        for (size_t a = 0; a < n_key_attr; ++a)
                            if (key_attribute_ids[a] == node->attribute())
                                nodelist[key_entries - ++filled] = node;
                
                const Node* node = c->make_tree_entry(key_entries, nodelist, &m_aggr_root_node);

                if (node)
                    nodeid_vec[n_nodes++] = node->id();
                else
                    Log(0).stream() << "aggregate: can't create node" << std::endl;
            }
        } else if (s_key_attribute_ids.size() == 0) {
            // --- no key attributes set: take nodes in snapshot   

            nodeid_vec = static_cast<cali_id_t*>(alloca((sizes.n_nodes+1) * sizeof(cali_id_t)));
            n_nodes    = sizes.n_nodes;
            
            for (size_t i = 0; i < sizes.n_nodes; ++i)
                nodeid_vec[i] = addr.node_entries[i]->id();

            // --- sort to make unique keys
            std::sort(nodeid_vec, nodeid_vec + sizes.n_nodes);
        }        
        
        //
        // --- encode key
        //

        // key encoding is as follows:
        //    - 1 u64: "toc" = 2 * num_nodes + (1 if immediate entries | 0 if no immediate entries)
        //    - num_nodes u64: key node ids
        //    - 1 u64: bitfield of indices in s_key_attributes that mark immediate key entries
        //    - for each immediate entry, 1 u64 entry for the value 

        // encode node key

        unsigned char   node_key[MAX_KEYLEN];
        size_t          node_key_len = 0;

        for (size_t i = 0; i < n_nodes; ++i) {
            size_t p = vlenc_u64(nodeid_vec[i], node_key + node_key_len);

            if (node_key_len + p + 1 >= MAX_KEYLEN) {
                ++m_num_skipped_keys;
                break;
            }

            node_key_len += p;
        }

        // encode selected immediate key entries

        unsigned char   imm_key[MAX_KEYLEN];
        size_t          imm_key_len = 0;
        uint64_t        imm_key_bitfield = 0;

        for (size_t k = 0; k < s_key_attribute_ids.size(); ++k) 
            for (size_t i = 0; i < sizes.n_immediate; ++i)
                if (s_key_attribute_ids[k] == addr.immediate_attr[i]) {
                    unsigned char buf[10];
                    size_t        p = 0;

                    p += vlenc_u64(*static_cast<const uint64_t*>(addr.immediate_data[i].data()), imm_key + imm_key_len);
                    
                    // check size and discard entry if it won't fit :(
                    if (node_key_len + imm_key_len + p + vlenc_u64(imm_key_bitfield | (1 << k), buf) + 1 >= MAX_KEYLEN) {
                        ++m_num_skipped_keys;
                        break;
                    }

                    imm_key_bitfield |= (1 << k);
                    imm_key_len      += p;
                }

        unsigned char   key[MAX_KEYLEN];
        size_t          pos = 0;

        pos += vlenc_u64(n_nodes * 2 + (imm_key_bitfield ? 1 : 0), key + pos);
        memcpy(key+pos, node_key, node_key_len);
        pos += node_key_len;

        if (imm_key_bitfield) {
            pos += vlenc_u64(imm_key_bitfield, key+pos);
            memcpy(key+pos, imm_key, imm_key_len);
            pos += imm_key_len;
        }

        m_max_keylen = std::max(pos, m_max_keylen);

        //
        // --- find entry
        //

        TrieNode* entry = find_entry(pos, key, !c->is_signal());

        if (!entry) {
            ++m_num_dropped;
            return;
        }

        //
        // --- update values
        //

        ++entry->count;

        for (size_t a = 0; a < s_aggr_attributes.size(); ++a)
            for (size_t i = 0; i < sizes.n_immediate; ++i)
                if (addr.immediate_attr[i] == s_aggr_attributes[a].id()) {
                    AggregateKernel* k = m_kernels.get(entry->k_id + a, !c->is_signal());

                    if (k)
                        k->add(addr.immediate_data[i].to_double());
                }
    }

    size_t flush(Caliper* c, Caliper::SnapshotFlushFn proc_fn) {
        TrieNode*     entry = m_trie.get(0, false);
        unsigned char key   = 0;

        return recursive_flush(0, &key, entry, c, proc_fn);
    }

    bool stopped() const {
        return m_stopped.load();
    }

    AggregateDB(Caliper* c)
        : m_stopped(false),
          m_retired(false),
          m_next(nullptr),
          m_prev(nullptr),
          m_aggr_root_node(CALI_INV_ID, CALI_INV_ID, Variant()),
          m_num_trie_entries(0),
          m_num_kernel_entries(0),
          m_num_dropped(0),
          m_num_skipped_keys(0),
          m_max_keylen(0)
    {
        Log(2).stream() << "Aggregate: creating aggregation database" << std::endl;

        // initialize first block
        m_trie.get(0, true);
        m_kernels.get(0, true);
    }

    ~AggregateDB() {
    }

    static AggregateDB* acquire(Caliper* c, bool alloc) {
        AggregateDB* db =
            static_cast<AggregateDB*>(pthread_getspecific(s_aggregate_db_key));

        if (alloc && !db) {
            db = new AggregateDB(c);

            if (pthread_setspecific(s_aggregate_db_key, db) == 0) {
                std::lock_guard<util::spinlock>
                    g(s_list_lock);

                if (s_list)
                    s_list->m_prev = db;

                db->m_next = s_list;
                s_list     = db;
            }
        }

        return db;
    }

    static void retire(void* ptr) {
        AggregateDB* db = static_cast<AggregateDB*>(ptr);

        if (!db)
            return;

        db->m_retired.store(true);
    }

    static void flush_cb(Caliper* c, const SnapshotRecord*, Caliper::SnapshotFlushFn proc_fn) {
        AggregateDB* db = nullptr;

        {
            std::lock_guard<util::spinlock>
                g(s_list_lock);

            db = s_list;
        }

        size_t num_written = 0;
        std::unordered_set<cali_id_t> written_node_cache;

        while (db) {
            db->m_stopped.store(true);
            num_written += db->flush(c, proc_fn);

            s_global_num_trie_entries   += db->m_num_trie_entries;
            s_global_num_kernel_entries += db->m_num_kernel_entries;
            s_global_num_trie_blocks    += db->m_trie.num_blocks();
            s_global_num_kernel_blocks  += db->m_kernels.num_blocks();
            s_global_num_skipped_keys   += db->m_num_skipped_keys;
            s_global_num_dropped        += db->m_num_dropped;
            s_global_max_keylen = std::max(s_global_max_keylen, db->m_max_keylen);
            
            db->clear();
            
            db->m_stopped.store(false);

            if (db->m_retired) {
                AggregateDB* tmp = db->m_next;

                {
                    std::lock_guard<util::spinlock>
                        g(s_list_lock);

                    db->unlink();

                    if (db == s_list)
                        s_list = tmp;
                }

                delete db;
                db = tmp;
            } else {
                db = db->m_next;
            }
        }

        Log(1).stream() << "Aggregate: flushed " << num_written << " snapshots." << std::endl;
    }

    static void process_snapshot_cb(Caliper* c, const SnapshotRecord* trigger_info, const SnapshotRecord* snapshot) {
        AggregateDB* db = acquire(c, !c->is_signal());

        if (db && !db->stopped())
            db->process_snapshot(c, snapshot);
        else
            ++s_global_num_dropped;
    }

    static void post_init_cb(Caliper* c) {
        // Update key attributes
        for (unsigned i = 0; i < s_key_attribute_names.size(); ++i) {
            Attribute attr = c->get_attribute(s_key_attribute_names[i]);

            if (attr != Attribute::invalid) {
                s_key_attributes[i]    = attr;
                s_key_attribute_ids[i] = attr.id();
            }
        }

        // Initialize aggregation attributes
        std::vector<std::string> names =
            s_config.get("attributes").to_stringlist(",:");

        init_aggregation_attributes(c, names);

        // Initialize master-thread aggregation DB
        acquire(c, true);
    }

    static void create_attribute_cb(Caliper* c, const Attribute& attr) {
        // Update key attributes
        auto it = std::find(s_key_attribute_names.begin(), s_key_attribute_names.end(),
                            attr.name());

        // No lock: hope that update is more-or-less atomic, and
        // consequences of invalid values are negligible
        if (it != s_key_attribute_names.end()) {
            if (attr.store_as_value()) {
                cali_attr_type type = attr.type();

                if (type != CALI_TYPE_INT  &&
                    type != CALI_TYPE_UINT &&
                    type != CALI_TYPE_ADDR &&
                    type != CALI_TYPE_BOOL &&   
                    type != CALI_TYPE_TYPE) {
                    Log(1).stream() << "Aggregate: warning: type " << cali_type2string(type)
                                    << " in as-value attribute \"" << attr.name() 
                                    << "\" is not supported in aggregation key and will be dropped." 
                                    << std::endl;
                    return;
                }
            }

            s_key_attributes[it-s_key_attribute_names.begin()]    = attr;
            s_key_attribute_ids[it-s_key_attribute_names.begin()] = attr.id();
        }
    }

    static void create_scope_cb(Caliper* c, cali_context_scope_t scope) {
        // create new aggregation DB on thread
        if (scope == CALI_SCOPE_THREAD)
            acquire(c, true);
    }

    static void finish_cb(Caliper* c) {
        if (Log::verbosity() >= 2) {
            unitfmt_result bytes_reserved = 
                unitfmt(s_global_num_trie_blocks * sizeof(TrieNode) * 1024
                        + s_global_num_kernel_blocks * sizeof(AggregateKernel) * 1024, unitfmt_bytes);

            Log(2).stream() << "Aggregate: max key len " << s_global_max_keylen << ", "
                            << s_global_num_kernel_entries << " entries, "
                            << s_global_num_trie_entries << " nodes, "
                            << s_global_num_trie_blocks + s_global_num_kernel_blocks << " blocks ("
                            << bytes_reserved.val << " " << bytes_reserved.symbol << " reserved)"
                            << std::endl;
        }

        // report attribute keys we haven't found 
        for (size_t i = 0; i < s_key_attribute_ids.size(); ++i)
            if (s_key_attribute_ids[i] == CALI_INV_ID)
                Log(1).stream() << "Aggregate: warning: key attribute \""
                                << s_key_attribute_names[i]
                                << "\" unused" << std::endl;

        if (s_global_num_dropped > 0)
            Log(1).stream() << "Aggregate: dropped " << s_global_num_dropped
                            << " snapshots." << std::endl;
        if (s_global_num_skipped_keys > 0)
            Log(0).stream() << "Aggregate: warning: maximum key length exceeded " 
                            << s_global_num_skipped_keys
                            << (s_global_num_skipped_keys == 1 ? " time!" : " times!")
                            << " Some key attributes could not be preserved."
                            << " Reduce number of aggregation key entries." << std::endl;
    }

    static void aggregate_register(Caliper* c) {
        if (!AggregateDB::init_static_data()) {
            Log(0).stream() << "aggregate: disabling aggregation service"
                            << std::endl;

            return;
        }

        c->events().create_attr_evt.connect(create_attribute_cb);
        c->events().post_init_evt.connect(post_init_cb);
        c->events().create_scope_evt.connect(create_scope_cb);
        c->events().process_snapshot.connect(process_snapshot_cb);
        c->events().flush_evt.connect(flush_cb);
        c->events().finish_evt.connect(finish_cb);

        Log(1).stream() << "Registered aggregation service" << std::endl;
    }

};

const ConfigSet::Entry AggregateDB::s_configdata[] = {
    { "attributes",   CALI_TYPE_STRING, "",
      "List of attributes to be aggregated",
      "List of attributes to be aggregated."
      "If specified, only aggregate the given attributes."
      "By default, aggregate all aggregatable attributes." },
    { "key",   CALI_TYPE_STRING, "",
      "List of attributes in the aggregation key",
      "List of attributes in the aggregation key."
      "If specified, only group by the given attributes." },
    ConfigSet::Terminator
};

ConfigSet      AggregateDB::s_config;

Attribute      AggregateDB::s_count_attribute = Attribute::invalid;

vector<string> AggregateDB::s_key_attribute_names;
vector<Attribute> AggregateDB::s_key_attributes;
vector<Attribute> AggregateDB::s_aggr_attributes;
vector<cali_id_t> AggregateDB::s_key_attribute_ids;
vector<AggregateDB::StatisticsAttributes> AggregateDB::s_stats_attributes;

pthread_key_t  AggregateDB::s_aggregate_db_key;

AggregateDB*   AggregateDB::s_list = nullptr;
util::spinlock AggregateDB::s_list_lock;

size_t         AggregateDB::s_global_num_trie_entries   = 0;
size_t         AggregateDB::s_global_num_kernel_entries = 0;
size_t         AggregateDB::s_global_num_trie_blocks    = 0;
size_t         AggregateDB::s_global_num_kernel_blocks  = 0;
size_t         AggregateDB::s_global_num_dropped        = 0;
size_t         AggregateDB::s_global_num_skipped_keys   = 0;
size_t         AggregateDB::s_global_max_keylen         = 0;

namespace cali
{
    CaliperService aggregate_service { "aggregate", &AggregateDB::aggregate_register };
}
