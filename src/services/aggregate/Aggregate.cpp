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

#include "AggregationDB.h"

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/Variant.h"

#include "caliper/common/c-util/unitfmt.h"

#include "caliper/common/util/spinlock.hpp"

#include <algorithm>
#include <cstring>
#include <mutex>

using namespace aggregate;
using namespace cali;
using namespace std;

namespace
{

//
// --- Per-experiment aggregator configuration
//

class Aggregate
{    
    //
    // --- Class for the per-thread aggregation database
    //

    //   ThreadDB manages an aggregation DB for one thread.
    // All ThreadDBs belonging to an experiment are linked so they
    // can be flushed, cleared, and deleted from a single thread.
    
    struct ThreadDB {
        //
        // --- members
        //

        std::atomic<bool> stopped;
        std::atomic<bool> retired;

        ThreadDB*         next = nullptr;
        ThreadDB*         prev = nullptr;

        AggregationDB     db;

        void unlink() {
            if (next)
                next->prev = prev;
            if (prev)
                prev->next = next;
        }

        ThreadDB()
            : stopped(false), retired(false)
            { }
        
    };

    //   ThreadData manages the static thread-local object. It contains
    // an array with each experiment's ThreadDBs for a thread.
    
    class ThreadData {
        std::vector<ThreadDB*> exp_thread_dbs;

    public:

        ~ThreadData() {
            for (ThreadDB* tdb : exp_thread_dbs)
                if (tdb)
                    tdb->retired.store(true);
        }

        ThreadDB* acquire_tdb(Aggregate* agg, Experiment* exp, bool alloc) {
            size_t    expI = exp->id();
            ThreadDB* tdb  = nullptr;

            if (expI < exp_thread_dbs.size())
                tdb = exp_thread_dbs[expI];

            if (!tdb && alloc) {
                tdb = new ThreadDB;

                if (exp_thread_dbs.size() <= expI)
                    exp_thread_dbs.resize(std::max<size_t>(16, expI));

                exp_thread_dbs[expI] = tdb;

                std::lock_guard<util::spinlock>
                    g(agg->tdb_lock);

                if (agg->tdb_list)
                    agg->tdb_list->prev = tdb;

                tdb->next     = agg->tdb_list;
                agg->tdb_list = tdb;
            }
        }
    };
    
    static thread_local ThreadData sT;

    static const ConfigSet::Entry  s_configdata[];

    ConfigSet                      config;

    ThreadDB*                      tdb_list;
    util::spinlock                 tdb_lock;

    AggregateAttributeInfo         aI;
    std::vector<std::string>       key_attribute_names;
    
    size_t num_trie_entries;
    size_t num_kernel_entries;
    size_t num_trie_blocks;
    size_t num_kernel_blocks;
    size_t num_dropped;
    size_t num_skipped_keys;
    size_t max_keylen;
    

    void init_aggregation_attributes(Caliper* c) {
        std::vector<std::string> aggr_attr_names =
            config.get("attributes").to_stringlist();
        
        if (aggr_attr_names.empty()) {
            // find all attributes of class "class.aggregatable"

            aI.aggr_attributes = 
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

                aI.aggr_attributes.push_back(attr);
            }
        }

        // Create the derived statistics attributes

        aI.stats_attributes.resize(aI.aggr_attributes.size());

        for (size_t i = 0; i < aI.aggr_attributes.size(); ++i) {
            std::string name = aI.aggr_attributes[i].name();

            aI.stats_attributes[i].min_attr =
                c->create_attribute(std::string("min#") + name,
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);
            aI.stats_attributes[i].max_attr =
                c->create_attribute(std::string("max#") + name,
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);
            aI.stats_attributes[i].sum_attr =
                c->create_attribute(std::string("sum#") + name,
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);            
            aI.stats_attributes[i].avg_attr =
                c->create_attribute(std::string("avg#") + name,
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);
        }

        aI.count_attribute =
            c->create_attribute("count",
                                CALI_TYPE_INT, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);
    }

    void flush_cb(Caliper* c, Experiment* exp, const SnapshotRecord*, SnapshotFlushFn proc_fn) {
        ThreadDB* tdb = nullptr;

        {
            std::lock_guard<util::spinlock>
                g(tdb_lock);

            tdb = tdb_list;
        }

        size_t num_written = 0;

        for ( ; tdb; tdb = tdb->next) {
            tdb->stopped.store(true);
            
            num_written        += tdb->db.flush(aI, c, proc_fn);

            num_trie_entries   += tdb->db.num_trie_entries();
            num_kernel_entries += tdb->db.num_kernel_entries();
//            num_trie_blocks    += db->m_trie.num_blocks();
//            num_kernel_blocks  += db->m_kernels.num_blocks();
            num_skipped_keys   += tdb->db.num_skipped_keys();
            num_dropped        += tdb->db.num_dropped();
            max_keylen          = std::max(max_keylen, tdb->db.max_keylen());

            tdb->stopped.store(false);
        }

        Log(1).stream() << exp->name() << ": Aggregate: flushed " << num_written << " snapshots." << std::endl;
    }

    void clear_cb(Caliper* c) {
        ThreadDB* tdb = nullptr;

        {
            std::lock_guard<util::spinlock>
                g(tdb_lock);

            tdb = tdb_list;
        }

        while (tdb) {
            tdb->stopped.store(true);
            tdb->db.clear();
            tdb->stopped.store(false);

            if (tdb->retired) {
                ThreadDB* tmp = tdb->next;

                {
                    std::lock_guard<util::spinlock>
                        g(tdb_lock);

                    tdb->unlink();

                    if (tdb == tdb_list)
                        tdb_list = tmp;
                }

                delete tdb;
                tdb = tmp;
            } else {
                tdb = tdb->next;
            }
        }        
    }

    void process_snapshot_cb(Caliper* c, Experiment* exp, const SnapshotRecord*, const SnapshotRecord* snapshot) {
        ThreadDB* tdb = sT.acquire_tdb(this, exp, !c->is_signal());

        if (tdb && !tdb->stopped.load())
            tdb->db.process_snapshot(aI, c, snapshot);
        else
            ++num_dropped;
    }

    void post_init_cb(Caliper* c, Experiment* exp) {
        // Update key attributes
        for (unsigned i = 0; i < key_attribute_names.size(); ++i) {
            Attribute attr = c->get_attribute(key_attribute_names[i]);

            if (attr != Attribute::invalid) {
                aI.key_attributes[i]    = attr;
                aI.key_attribute_ids[i] = attr.id();
            }
        }

        init_aggregation_attributes(c);

        // Initialize master-thread aggregation DB
        sT.acquire_tdb(this, exp, true);
    }

    void create_attribute_cb(Caliper* c, const Attribute& attr) {
        // Update key attributes
        auto it = std::find(key_attribute_names.begin(), key_attribute_names.end(),
                            attr.name());

        // No lock: hope that update is more-or-less atomic, and
        // consequences of invalid values are negligible
        if (it != key_attribute_names.end()) {
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

            auto index = it - key_attribute_names.begin();

            aI.key_attributes   [index] = attr;
            aI.key_attribute_ids[index] = attr.id();
        }
    }

    void create_thread_cb(Caliper* c, Experiment* exp) {
        sT.acquire_tdb(this, exp, true);
    }

    void finish_cb(Caliper* c, Experiment* exp) {
        if (Log::verbosity() >= 2) {
            // unitfmt_result bytes_reserved = 
            //     unitfmt(num_trie_blocks * sizeof(TrieNode) * 1024
            //             + num_kernel_blocks * sizeof(AggregateKernel) * 1024, unitfmt_bytes);

            // Log(2).stream() << exp->name() << ": Aggregate: max key len " << max_keylen << ", "
            //                 << num_kernel_entries << " entries, "
            //                 << num_trie_entries << " nodes, "
            //                 << num_trie_blocks + num_kernel_blocks << " blocks ("
            //                 << bytes_reserved.val << " " << bytes_reserved.symbol << " reserved)"
            //                 << std::endl;
        }

        // report attribute keys we haven't found 
        for (size_t i = 0; i < aI.key_attribute_ids.size(); ++i)
            if (aI.key_attribute_ids[i] == CALI_INV_ID)
                Log(1).stream() << exp->name() << ": Aggregate: warning: key attribute \""
                                << key_attribute_names[i]
                                << "\" unused" << std::endl;

        if (num_dropped > 0)
            Log(1).stream() << exp->name() << ": Aggregate: dropped " << num_dropped
                            << " snapshots." << std::endl;
        if (num_skipped_keys > 0)
            Log(0).stream() << exp->name() << ": Aggregate: warning: maximum key length exceeded " 
                            << num_skipped_keys
                            << (num_skipped_keys == 1 ? " time!" : " times!")
                            << " Some key attributes could not be preserved."
                            << " Reduce number of aggregation key entries." << std::endl;
    }

    Aggregate(Caliper* c, Experiment* exp)
        : config(exp->config().init("aggregate", s_configdata))
        {
            tdb_lock.unlock();

            key_attribute_names = config.get("key").to_stringlist(",:");

            aI.key_attribute_ids.assign(key_attribute_names.size(), CALI_INV_ID);
            aI.key_attributes.assign(key_attribute_names.size(), Attribute::invalid);
        }
        
public:

    ~Aggregate() {
        ThreadDB* tdb = tdb_list;

        while (tdb) {
            ThreadDB* tmp = tdb->next;

            tdb->unlink();

            if (tdb == tdb_list)
                tdb_list = tmp;

            delete tdb;
            tdb = tmp;
            
        }
    }
    
    static void aggregate_register(Caliper* c, Experiment* exp) {
        Aggregate* instance = new Aggregate(c, exp);

        exp->events().create_attr_evt.connect(
            [instance](Caliper* c, Experiment*, const Attribute& attr){
                instance->create_attribute_cb(c, attr);
            });
        exp->events().post_init_evt.connect(
            [instance](Caliper* c, Experiment* exp){
                instance->post_init_cb(c, exp);
            });
        exp->events().create_thread_evt.connect(
            [instance](Caliper* c, Experiment* exp){
                instance->create_thread_cb(c, exp);
            });
        exp->events().process_snapshot.connect(
            [instance](Caliper* c, Experiment* exp, const SnapshotRecord* info, const SnapshotRecord* snapshot){
                instance->process_snapshot_cb(c, exp, info, snapshot);
            });
        exp->events().flush_evt.connect(
            [instance](Caliper* c, Experiment* exp, const SnapshotRecord* info, SnapshotFlushFn proc_fn){
                instance->flush_cb(c, exp, info, proc_fn);
            });
        exp->events().clear_evt.connect(
            [instance](Caliper* c, Experiment*){
                instance->clear_cb(c);
            });
        exp->events().finish_evt.connect(
            [instance](Caliper* c, Experiment* exp){
                instance->finish_cb(c, exp);
                delete instance;
            });

        Log(1).stream() << exp->name() << ": Registered aggregation service" << std::endl;
    }
    
}; // class Aggregate

const ConfigSet::Entry Aggregate::s_configdata[] = {
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

thread_local Aggregate::ThreadData Aggregate::sT;

} // namespace [anonymous]


namespace cali
{

CaliperService aggregate_service { "aggregate", ::Aggregate::aggregate_register };

}
