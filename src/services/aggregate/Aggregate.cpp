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

#include "../CaliperService.h"

#include <Caliper.h>
#include <EntryList.h>

#include <ContextRecord.h>
#include <Log.h>
#include <Node.h>
#include <RuntimeConfig.h>
#include <Variant.h>

#include <c-util/vlenc.h>

#include <util/spinlock.hpp>
#include <util/split.hpp>

#include <pthread.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <mutex>
#include <vector>
#include <unordered_set>

using namespace cali;

#define MAX_KEYLEN         128
#define MAX_BLOCKS        2048
#define ENTRIES_PER_BLOCK 2048

#define SNAP_MAX            80 // max snapshot size
#define AGGR_MAX             8 // max aggregation variables
    
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

    std::vector<Attribute>   m_aggr_attributes;

    // the actual aggregation db

    struct Statistics  {
        double   min;
        double   max;
        double   sum;
        int      count;
            
        Statistics()
            : min(std::numeric_limits<double>::max()),
              max(std::numeric_limits<double>::min()),
              sum(0), count(0)
        { }

        void add(double val) {
            min  = std::min(min, val);
            max  = std::max(max, val);
            sum += val;
            ++count;
        }            
    };

    struct Entry {
        uint32_t     next[256] = { 0 };
        Statistics values[AGGR_MAX];
    };

    Entry*                   m_blocks[MAX_BLOCKS] = { nullptr };
        
    // we maintain some internal statistics        
    size_t                   m_num_entries;
    size_t                   m_num_blocks;
    size_t                   m_num_dropped;
    size_t                   m_max_keylen;        
        
    //
    // --- static data
    //

    struct StatisticsAttributes {
        Attribute min_attr;
        Attribute max_attr;
        Attribute sum_attr;
        Attribute count_attr;
    };

    static std::vector<std::string>
                             s_aggr_attribute_names;
    static std::vector<StatisticsAttributes>
                             s_stats_attributes;
        
    static const ConfigSet::Entry
                             s_configdata[];
    static ConfigSet         s_config;

    static pthread_key_t     s_aggregate_db_key;

    static AggregateDB*      s_list;
    static util::spinlock    s_list_lock;

    // global statistics
    static size_t            s_global_num_entries;
    static size_t            s_global_num_blocks;
    static size_t            s_global_num_dropped;
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
        
    Entry* get_entry(uint32_t id, bool alloc) {
        size_t block = id / ENTRIES_PER_BLOCK;

        if (block > MAX_BLOCKS)
            return 0;

        if (!m_blocks[block]) {
            if (alloc) {
                m_blocks[block] = new Entry[ENTRIES_PER_BLOCK];
                ++m_num_blocks;
            } else
                return 0;
        }
            
        return m_blocks[block] + (id % ENTRIES_PER_BLOCK);
    }

    Entry* find_entry(size_t n, unsigned char* key, bool alloc) {
        Entry* entry = get_entry(0, alloc);

        for ( size_t i = 0; entry && i < n; ++i ) {
            uint32_t id = entry->next[key[i]];

            if (!id) {
                id = static_cast<uint32_t>(++m_num_entries);
                entry->next[key[i]] = id;
            }

            entry = get_entry(id, alloc);
        }

        return entry;
    }

    void write_aggregated_snapshot(const unsigned char* key, const Statistics* values,
                                   Caliper* c, std::unordered_set<cali_id_t>& written_node_cache) {
        // --- decode key

        size_t   p = 0;            
        int      num_nodes = static_cast<int>(vldec_u64(key + p, &p));
            
        Variant  node_vec[SNAP_MAX];

        for (int i = 0; i < num_nodes; ++i)
            node_vec[i] = Variant(static_cast<cali_id_t>(vldec_u64(key + p, &p)));

        // --- write aggregate entries

        int      num_aggr_attr = std::min(AGGR_MAX, static_cast<int>(m_aggr_attributes.size()));
            
        Variant  attr_vec[SNAP_MAX];
        Variant  data_vec[SNAP_MAX];

        int      ap = 0;

        for (int a = 0; a < num_aggr_attr; ++a) {
            if (values[a].count == 0)
                continue;

            attr_vec[4*ap+0] = Variant(s_stats_attributes[a].min_attr.id());
            attr_vec[4*ap+1] = Variant(s_stats_attributes[a].max_attr.id());
            attr_vec[4*ap+2] = Variant(s_stats_attributes[a].sum_attr.id());
            attr_vec[4*ap+3] = Variant(s_stats_attributes[a].count_attr.id());

            int64_t countval = values[a].count;
                
            data_vec[4*ap+0] = Variant(values[a].min);
            data_vec[4*ap+1] = Variant(values[a].max);
            data_vec[4*ap+2] = Variant(values[a].sum);
            data_vec[4*ap+3] = Variant(CALI_TYPE_INT, &countval, sizeof(int64_t));

            ++ap;
        }

        int      num_immediate = 4*ap;

        // --- write nodes (FIXME: get rid of this awful node cache hack)

        for (int i = 0; i < num_nodes; ++i) {
            cali_id_t node_id = node_vec[i].to_id();

            if (written_node_cache.count(node_id))
                continue;

            Node* node = c->node(node_id);

            if (node)
                node->write_path(c->events().write_record);
            else
                Log(2).stream() << "aggregate: error: unknown node id " << node_id << std::endl;
        }
        for (int i = 0; i < num_immediate; ++i) {
            cali_id_t node_id = attr_vec[i].to_id();

            if (written_node_cache.count(node_id))
                continue;

            Node* node = c->node(node_id);

            if (node)
                node->write_path(c->events().write_record);
            else
                Log(2).stream() << "aggregate: error: unknown node id " << node_id << std::endl;
        }

        // --- write snapshot record

        int               n[3] = { num_nodes, num_immediate, num_immediate };
        const Variant* data[3] = { node_vec,  attr_vec,      data_vec      };

        c->events().write_record(ContextRecord::record_descriptor(), n, data);
    }
        
    size_t recursive_flush(size_t n, unsigned char* key, Entry* entry,
                           Caliper* c, std::unordered_set<cali_id_t>& written_node_cache) {
        if (!entry)
            return 0;

        size_t num_written = 0;

        // --- write current entry if it represents a snapshot

        size_t num_aggr_attr = std::max<size_t>(AGGR_MAX, m_aggr_attributes.size());
        int    count = 0;

        for (size_t a = 0; count == 0 && a < num_aggr_attr; ++a)
            count += entry->values[a].count;
            
        if (count > 0)
            write_aggregated_snapshot(key, entry->values, c, written_node_cache);

        num_written += (count > 0 ? 1 : 0);

        // --- iterate over sub-records

        unsigned char* next_key = static_cast<unsigned char*>(alloca(n+2));

        memset(next_key, 0, n+2);
        memcpy(next_key, key, n);
        
        for (size_t i = 0; i < 256; ++i) {
            if (entry->next[i] == 0)
                continue;

            Entry* e     = get_entry(entry->next[i], false);
            next_key[n]  = static_cast<unsigned char>(i);

            num_written += recursive_flush(n+1, next_key, e, c, written_node_cache);
        }

        return num_written;
    }
        
public:

    void clear() {
        for (int i = 0; i < MAX_BLOCKS && m_blocks[i]; ++i) {
            delete[] m_blocks[i];
            m_blocks[i] = 0;
        }

        m_num_entries = 0;
        m_num_blocks  = 0;
        m_num_dropped = 0;
        m_max_keylen  = 0;
    }
    
    void process_snapshot(Caliper* c, const EntryList* snapshot) {
        EntryList::Sizes sizes = snapshot->size();

        if (sizes.n_nodes + sizes.n_immediate == 0)
            return;

        EntryList::Data addr   = snapshot->data();

        //
        // --- extract aggregation variables
        //
            
        double          values[AGGR_MAX] = { 0     };
        bool            found [AGGR_MAX] = { false };
        bool            found_any        = false;

        for ( size_t a = 0; a < AGGR_MAX && a < m_aggr_attributes.size(); ++a ) {
            for (size_t i = 0; i < sizes.n_immediate; ++i) {
                if (m_aggr_attributes[a].id() == addr.immediate_attr[i]) {
                    values[a] = addr.immediate_data[i].to_double();
                    found[a]  = true;
                    found_any = true;
                    break;
                }
            }
        }

        if (!found_any)
            return;
            
        //
        // --- sort entries in node vector to normalize key
        //
            
        cali_id_t       nodeid_vec[SNAP_MAX];

        for (size_t i = 0; i < sizes.n_nodes; ++i)
            nodeid_vec[i] = addr.node_entries[i]->id();
            
        std::sort(nodeid_vec, nodeid_vec + sizes.n_nodes);            

        //
        // --- encode key
        //
            
        unsigned char   key[MAX_KEYLEN];
            
        uint64_t        n_nodes = sizes.n_nodes;
        size_t          pos     = 0;

        pos += vlenc_u64(n_nodes, key + pos);

        for (size_t i = 0; i < sizes.n_nodes && pos+10 < MAX_KEYLEN; ++i)
            pos += vlenc_u64(nodeid_vec[i], key + pos);

        m_max_keylen = std::max(pos, m_max_keylen);

        //
        // --- find entry
        //

        Entry* entry = find_entry(pos, key, !c->is_signal());

        if (!entry) {
            ++m_num_dropped;
            return;
        }

        //
        // --- update values
        //

        for ( size_t a = 0; a < AGGR_MAX && a < m_aggr_attributes.size(); ++a )
            if (found[a]) 
                entry->values[a].add(values[a]);
    }

    size_t flush(Caliper* c, std::unordered_set<cali_id_t>& written_node_cache) {
        Entry*        entry = get_entry(0, false);
        unsigned char key   = 0;

        return recursive_flush(0, &key, entry, c, written_node_cache);
    }

    bool stopped() const {
        return m_stopped.load();
    }

    AggregateDB(Caliper* c)
        : m_stopped(false),
          m_retired(false),
          m_next(nullptr),
          m_prev(nullptr),
          m_num_entries(0),
          m_num_blocks(0),
          m_num_dropped(0),
          m_max_keylen(0)
    {
        m_aggr_attributes.assign(s_aggr_attribute_names.size(), Attribute::invalid);

        Log(2).stream() << "aggregate: creating aggregation database" << std::endl;
            
        for (int a = 0; a < AGGR_MAX && a < s_aggr_attribute_names.size(); ++a) {
            Attribute attr = c->get_attribute(s_aggr_attribute_names[a]);

            if (attr == Attribute::invalid)
                Log(1).stream() << "aggregate: warning: aggregation attribute "
                                << s_aggr_attribute_names[a]
                                << " not found" << std::endl;
            else
                m_aggr_attributes[a] = attr;
        }

        // initialize first block
        get_entry(0, true);
    }

    ~AggregateDB() {
        for (int i = 0; i < MAX_BLOCKS && m_blocks[i]; ++i)
            delete[] m_blocks[i];
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

    static void flush_cb(Caliper* c, const EntryList*) {
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
            num_written += db->flush(c, written_node_cache);

            s_global_num_entries += db->m_num_entries;
            s_global_num_blocks  += db->m_num_blocks;
            s_global_num_dropped += db->m_num_dropped;
            s_global_max_keylen   = std::max(s_global_max_keylen, db->m_max_keylen);

            db->m_stopped.store(false);

            db->clear();

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

        Log(1).stream() << "aggregate: flushed " << num_written << " snapshots." << std::endl;
    }
        
    static void process_snapshot_cb(Caliper* c, const EntryList* trigger_info, const EntryList* snapshot) {
        AggregateDB* db = AggregateDB::acquire(c, !c->is_signal());

        if (db && !db->stopped())
            db->process_snapshot(c, snapshot);
        else
            ++s_global_num_dropped;
    }

    static void post_init_cb(Caliper* c) {        
        // Initialize master-thread aggregation DB
        AggregateDB::acquire(c, true);
    }

    static void finish_cb(Caliper* c) {
        Log(2).stream() << "aggregate: runtime statistics:"
                        << "\n  max key len: " << s_global_max_keylen
                        << "\n  num entries: " << s_global_num_entries
                        << "\n  num blocks:  " << s_global_num_blocks
                        << " (" << s_global_num_blocks * sizeof(Entry) * ENTRIES_PER_BLOCK << " bytes)"
                        << std::endl;
            
        if (s_global_num_dropped > 0)
            Log(1).stream() << "aggregate: dropped " << s_global_num_dropped
                            << " snapshots." << std::endl;
    }

    static void create_statistics_attributes(Caliper* c) {
        s_stats_attributes.resize(s_aggr_attribute_names.size());
            
        for (size_t i = 0; i < s_aggr_attribute_names.size(); ++i) {
            s_stats_attributes[i].min_attr =
                c->create_attribute(std::string("aggregate.min*") + s_aggr_attribute_names[i],
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);
            s_stats_attributes[i].max_attr =
                c->create_attribute(std::string("aggregate.max*") + s_aggr_attribute_names[i],
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);
            s_stats_attributes[i].sum_attr =
                c->create_attribute(std::string("aggregate.sum*") + s_aggr_attribute_names[i],
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);
            s_stats_attributes[i].count_attr =
                c->create_attribute(std::string("aggregate.count*") + s_aggr_attribute_names[i],
                                    CALI_TYPE_INT,    CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);                
        }
    }

    static bool init_static_data() {
        s_list_lock.unlock();

        s_config = RuntimeConfig::init("aggregate", s_configdata);

        if (s_config.get("attributes").to_string().size() < 1) {
            Log(0).stream() << "aggregate: no aggregation attributes set"
                            << std::endl;
            return false;
        }

        util::split(s_config.get("attributes").to_string(), ':', 
                    std::back_inserter(s_aggr_attribute_names));

        if (pthread_key_create(&s_aggregate_db_key, retire) != 0) {
            Log(0).stream() << "aggregate: error: pthread_key_create() failed" 
                            << std::endl;
            return false;
        }

        return true;
    }
        
    static void aggregate_register(Caliper* c) {
        if (!AggregateDB::init_static_data()) {
            Log(0).stream() << "aggregate: disabling aggregation service"
                            << std::endl;
                
            return;
        }
            
        AggregateDB::create_statistics_attributes(c);

        c->events().post_init_evt.connect(post_init_cb);
        c->events().process_snapshot.connect(process_snapshot_cb);
        c->events().flush.connect(flush_cb);
        c->events().finish_evt.connect(finish_cb);

        Log(1).stream() << "Registered aggregation service" << std::endl;
    }
        
};

const ConfigSet::Entry AggregateDB::s_configdata[] = {
    { "attributes",   CALI_TYPE_STRING, "time.inclusive.duration",
      "List of attributes to be aggregated",
      "List of attributes to be aggregated" },        
    ConfigSet::Terminator
};

ConfigSet      AggregateDB::s_config;

std::vector<std::string> AggregateDB::s_aggr_attribute_names;
std::vector<AggregateDB::StatisticsAttributes> AggregateDB::s_stats_attributes;

pthread_key_t  AggregateDB::s_aggregate_db_key;

AggregateDB*   AggregateDB::s_list = nullptr;
util::spinlock AggregateDB::s_list_lock;

size_t         AggregateDB::s_global_num_entries = 0;
size_t         AggregateDB::s_global_num_blocks  = 0;
size_t         AggregateDB::s_global_num_dropped = 0;
size_t         AggregateDB::s_global_max_keylen  = 0;

namespace cali
{
    CaliperService AggregateService { "aggregate", &AggregateDB::aggregate_register };
}
