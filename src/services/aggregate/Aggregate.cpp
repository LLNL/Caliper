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

    AttributeInfo                  info;
    std::vector<std::string>       key_attribute_names;

    bool                           implicit_grouping;
    bool                           group_nested;

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

            info.aggr_attrs =
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

                info.aggr_attrs.push_back(attr);
            }
        }

        // Create the derived statistics attributes

        info.result_attrs.resize(info.aggr_attrs.size());

        for (size_t i = 0; i < info.aggr_attrs.size(); ++i) {
            std::string name = info.aggr_attrs[i].name();

            info.result_attrs[i].min_attr =
                c->create_attribute(std::string("min#") + name,
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
            info.result_attrs[i].max_attr =
                c->create_attribute(std::string("max#") + name,
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
            info.result_attrs[i].sum_attr =
                c->create_attribute(std::string("sum#") + name,
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
            info.result_attrs[i].avg_attr =
                c->create_attribute(std::string("avg#") + name,
                                    CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
#ifdef CALIPER_ENABLE_HISTOGRAMS
            for (int jj = 0; jj < CALI_AGG_HISTOGRAM_BINS; jj++) {
                info.result_attrs[i].histogram_attr[jj] =
                   c->create_attribute(std::string("histogram.bin.") + std::to_string(jj) + std::string("#") + name,
                                       CALI_TYPE_INT, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD);
            }
#endif
        }

        info.count_attr =
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
            num_written += tdb->db.flush(info, c, proc_fn);
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

    void process_snapshot_cb(Caliper* c, Channel* chn, const SnapshotRecord*, const SnapshotRecord* rec) {
        ThreadDB* tdb = acquire_tdb(c, chn, !c->is_signal());

        if (tdb && !tdb->stopped.load())
            tdb->db.process_snapshot(c, rec, info, implicit_grouping);
        else
            ++num_dropped_snapshots;
    }

    void check_key_attribute(const Attribute& attr) {
        auto it = std::find(key_attribute_names.begin(), key_attribute_names.end(),
                            attr.name());

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

                info.imm_key_attrs.push_back(attr);
            } else {
                info.ref_key_attrs.push_back(attr);
            }

            key_attribute_names.erase(it);
        } else if (group_nested && attr.is_nested())
            info.ref_key_attrs.push_back(attr);
    }

    void post_init_cb(Caliper* c, Channel* chn) {
        // Update key attributes
        for (const Attribute& a : c->get_all_attributes())
            check_key_attribute(a);

        init_aggregation_attributes(c);

        // Initialize master-thread aggregation DB
        acquire_tdb(c, chn, true);
    }

    void create_attribute_cb(Caliper*, const Attribute& attr) {
        check_key_attribute(attr);
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
        for (const std::string& s : key_attribute_names)
            Log(1).stream() << chn->name() << ": Aggregate: warning: key attribute \""
                            << s
                            << "\" unused" << std::endl;

        if (num_dropped_snapshots > 0)
            Log(1).stream() << chn->name() << ": Aggregate: dropped " << num_dropped_snapshots
                            << " snapshots." << std::endl;
    }

    void apply_key_config() {
        if (key_attribute_names.empty()) {
            implicit_grouping = true;
            return;
        }

        implicit_grouping = false;

        // Check for "*" and "prop:nested" special arguments to determine
        // nested or implicit grouping

        {
            auto it = std::find(key_attribute_names.begin(), key_attribute_names.end(), "*");

            if (it != key_attribute_names.end()) {
                implicit_grouping = true;
                key_attribute_names.erase(it);
            }
        }

        {
            auto it = std::find(key_attribute_names.begin(), key_attribute_names.end(), "prop:nested");

            if (it != key_attribute_names.end()) {
                group_nested = true;
                key_attribute_names.erase(it);
            }
        }
    }

    Aggregate(Caliper* c, Channel* chn)
        : config(chn->config().init("aggregate", s_configdata)),
          implicit_grouping(true),
          group_nested(false),
          num_dropped_snapshots(0)
        {
            tdb_lock.unlock();

            key_attribute_names = config.get("key").to_stringlist(",");
            apply_key_config();

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
