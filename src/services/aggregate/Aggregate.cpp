// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Caliper on-line aggregation service

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
#include <sstream>

using namespace aggregate;
using namespace cali;
using namespace std;

namespace
{

//
// --- Per-channel aggregator configuration
//

class Aggregate
{
    //
    // --- Class for the per-thread aggregation database
    //

    //   ThreadDB manages an aggregation DB for one thread.
    // All ThreadDBs belonging to a channel are linked so they
    // can be flushed, cleared, and deleted from any thread.

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

    static const ConfigSet::Entry  s_configdata[];

    ConfigSet                      config;

    ThreadDB*                      tdb_list = nullptr;
    util::spinlock                 tdb_lock;

    AggregateAttributeInfo         aI;
    std::vector<std::string>       key_attribute_names;

    Attribute                      tdb_attr;

    size_t                         num_dropped_snapshots;

    ThreadDB* acquire_tdb(Caliper* c, Channel* chn, bool can_alloc) {
        //   we store a pointer to the thread-local aggregation DB for this channel
        // on the thread's blackboard

        ThreadDB* tdb =
            static_cast<ThreadDB*>(c->get(tdb_attr).value().get_ptr());

        if (!tdb && can_alloc) {
            tdb = new ThreadDB;

            c->set(tdb_attr, Variant(cali_make_variant_from_ptr(tdb)));

            std::lock_guard<util::spinlock>
                g(tdb_lock);

            if (tdb_list)
                tdb_list->prev = tdb;

            tdb->next = tdb_list;
            tdb_list  = tdb;
        }

        return tdb;
    }

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
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
            aI.stats_attributes[i].max_attr =
                c->create_attribute(std::string("max#") + name,
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
            aI.stats_attributes[i].sum_attr =
                c->create_attribute(std::string("sum#") + name,
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
            aI.stats_attributes[i].avg_attr =
                c->create_attribute(std::string("avg#") + name,
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
#ifdef CALIPER_ENABLE_HISTOGRAMS
            for (int jj = 0; jj < CALI_AGG_HISTOGRAM_BINS; jj++) {
                aI.stats_attributes[i].histogram_attr[jj] =
                   c->create_attribute(std::string("histogram.bin.") + std::to_string(jj) + std::string("#") + name,
                                       CALI_TYPE_INT, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);
            }
#endif
        }

        aI.count_attribute =
            c->create_attribute("count",
                                CALI_TYPE_INT, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
    }

    void flush_cb(Caliper* c, Channel* chn, const SnapshotRecord*, SnapshotFlushFn proc_fn) {
        ThreadDB* tdb = nullptr;

        {
            std::lock_guard<util::spinlock>
                g(tdb_lock);

            tdb = tdb_list;
        }

        size_t num_written = 0;

        for ( ; tdb; tdb = tdb->next) {
            tdb->stopped.store(true);
            num_written += tdb->db.flush(aI, c, proc_fn);
            tdb->stopped.store(false);
        }

        Log(1).stream() << chn->name() << ": Aggregate: flushed " << num_written << " snapshots." << std::endl;
    }

    void clear_cb(Caliper* c, Channel* chn) {
        ThreadDB* tdb = nullptr;

        {
            std::lock_guard<util::spinlock>
                g(tdb_lock);

            tdb = tdb_list;
        }

        size_t num_trie_entries    = 0;
        size_t num_kernel_entries  = 0;
        size_t num_trie_blocks     = 0;
        size_t num_kernel_blocks   = 0;
        size_t bytes_reserved      = 0;
        size_t num_skipped_keys    = 0;
        size_t num_dropped_entries = 0;
        size_t max_keylen          = 0;

        while (tdb) {
            tdb->stopped.store(true);

            num_trie_entries    += tdb->db.num_trie_entries();
            num_kernel_entries  += tdb->db.num_kernel_entries();
            num_trie_blocks     += tdb->db.num_trie_blocks();
            num_kernel_blocks   += tdb->db.num_kernel_blocks();
            bytes_reserved      += tdb->db.bytes_reserved();
            num_skipped_keys    += tdb->db.num_skipped_keys();
            num_dropped_entries += tdb->db.num_dropped();
            max_keylen           = std::max(max_keylen, tdb->db.max_keylen());

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

        if (Log::verbosity() >= 2) {
            unitfmt_result bytes_reserved_fmt =
                unitfmt(bytes_reserved, unitfmt_bytes);

            Log(2).stream() << chn->name() << ": Aggregate: Releasing aggregation DB:\n    max key len "
                            << max_keylen << ", "
                            << num_kernel_entries << " entries, "
                            << num_trie_entries << " nodes, "
                            << num_trie_blocks + num_kernel_blocks << " blocks ("
                            << bytes_reserved_fmt.val << " " << bytes_reserved_fmt.symbol << " reserved)"
                            << std::endl;
        }

        if (num_skipped_keys > 0)
            Log(1).stream() << chn->name() << ": Aggregate: Dropped " << num_skipped_keys
                            << " entries because key exceeded max length!"
                            << std::endl;
        if (num_dropped_entries > 0)
            Log(1).stream() << chn->name() << ": Aggregate: Dropped " << num_dropped_entries
                            << " entries because key could not be allocated!"
                            << std::endl;
    }

    void process_snapshot_cb(Caliper* c, Channel* chn, const SnapshotRecord*, const SnapshotRecord* snapshot) {
        ThreadDB* tdb = acquire_tdb(c, chn, !c->is_signal());

        if (tdb && !tdb->stopped.load())
            tdb->db.process_snapshot(aI, c, snapshot);
        else
            ++num_dropped_snapshots;
    }

    void post_init_cb(Caliper* c, Channel* chn) {
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
        acquire_tdb(c, chn, true);
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

    void create_thread_cb(Caliper* c, Channel* chn) {
        acquire_tdb(c, chn, true);
    }

    void release_thread_cb(Caliper* c, Channel* chn) {
        ThreadDB* tdb = acquire_tdb(c, chn, false);

        if (tdb)
            tdb->retired.store(true);
    }

    void finish_cb(Caliper* c, Channel* chn) {
        // report attribute keys we haven't found
        for (size_t i = 0; i < aI.key_attribute_ids.size(); ++i)
            if (aI.key_attribute_ids[i] == CALI_INV_ID)
                Log(1).stream() << chn->name() << ": Aggregate: warning: key attribute \""
                                << key_attribute_names[i]
                                << "\" unused" << std::endl;

        if (num_dropped_snapshots > 0)
            Log(1).stream() << chn->name() << ": Aggregate: dropped " << num_dropped_snapshots
                            << " snapshots." << std::endl;
    }

    Aggregate(Caliper* c, Channel* chn)
        : config(chn->config().init("aggregate", s_configdata)),
          num_dropped_snapshots(0)
        {
            tdb_lock.unlock();

            key_attribute_names = config.get("key").to_stringlist(",:");

            aI.key_attribute_ids.assign(key_attribute_names.size(), CALI_INV_ID);
            aI.key_attributes.assign(key_attribute_names.size(), Attribute::invalid);

            tdb_attr =
                c->create_attribute(std::string("aggregate.tdb.") + std::to_string(chn->id()),
                                    CALI_TYPE_PTR,
                                    CALI_ATTR_SCOPE_THREAD |
                                    CALI_ATTR_ASVALUE      |
                                    CALI_ATTR_SKIP_EVENTS  |
                                    CALI_ATTR_HIDDEN);
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

    static void aggregate_register(Caliper* c, Channel* chn) {
        Aggregate* instance = new Aggregate(c, chn);

        chn->events().create_attr_evt.connect(
            [instance](Caliper* c, Channel*, const Attribute& attr){
                instance->create_attribute_cb(c, attr);
            });
        chn->events().post_init_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->post_init_cb(c, chn);
            });
        chn->events().create_thread_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->create_thread_cb(c, chn);
            });
        chn->events().release_thread_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->release_thread_cb(c, chn);
            });
        chn->events().process_snapshot.connect(
            [instance](Caliper* c, Channel* chn, const SnapshotRecord* info, const SnapshotRecord* snapshot){
                instance->process_snapshot_cb(c, chn, info, snapshot);
            });
        chn->events().flush_evt.connect(
            [instance](Caliper* c, Channel* chn, const SnapshotRecord* info, SnapshotFlushFn proc_fn){
                instance->flush_cb(c, chn, info, proc_fn);
            });
        chn->events().clear_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->clear_cb(c, chn);
            });
        chn->events().finish_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->clear_cb(c, chn); // prints logs
                instance->finish_cb(c, chn);
                delete instance;
            });

        Log(1).stream() << chn->name() << ": Registered aggregation service" << std::endl;
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

} // namespace [anonymous]


namespace cali
{

CaliperService aggregate_service { "aggregate", ::Aggregate::aggregate_register };

}
