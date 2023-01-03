// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Caliper on-line aggregation service

#include "AggregationDB.h"

#include "caliper/CaliperService.h"
#include "../Services.h"

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

        ThreadDB(Caliper* c, const AttributeInfo& info)
            : stopped(false), retired(false), db(c, info)
            { }
    };

    ConfigSet                      config;

    ThreadDB*                      tdb_list = nullptr;
    util::spinlock                 tdb_lock;

    AttributeInfo                  info;
    std::vector<std::string>       key_attribute_names;
    std::vector<std::string>       aggr_attribute_names;

    Attribute                      tdb_attr;

    size_t                         num_dropped_snapshots;

    ThreadDB* acquire_tdb(Caliper* c, Channel* chn, bool can_alloc) {
        //   we store a pointer to the thread-local aggregation DB for this channel
        // on the thread's blackboard

        ThreadDB* tdb =
            static_cast<ThreadDB*>(c->get(tdb_attr).value().get_ptr());

        if (!tdb && can_alloc) {
            tdb = new ThreadDB(c, info);

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

    ResultAttributes make_result_attributes(Caliper* c, const Attribute& attr) {
        std::string name = attr.name();
        ResultAttributes res;

        int prop = CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS;

        res.min_attr = c->create_attribute(std::string("min#") + name, CALI_TYPE_DOUBLE, prop);
        res.max_attr = c->create_attribute(std::string("max#") + name, CALI_TYPE_DOUBLE, prop);
        res.sum_attr = c->create_attribute(std::string("sum#") + name, CALI_TYPE_DOUBLE, prop);
        res.avg_attr = c->create_attribute(std::string("avg#") + name, CALI_TYPE_DOUBLE, prop);
    #ifdef CALIPER_ENABLE_HISTOGRAMS
        for (int jj = 0; jj < CALI_AGG_HISTOGRAM_BINS; jj++) {
            res.histogram_attr[jj] =
                c->create_attribute(std::string("histogram.bin.") + std::to_string(jj) + std::string("#") + name,
                                    CALI_TYPE_INT, prop);
        }
    #endif

        return res;
    }

    void check_aggregation_attribute(Caliper* c, const Attribute& attr) {
        if (!(attr.properties() & CALI_ATTR_AGGREGATABLE))
            return;

        if (std::find(info.aggr_attrs.begin(), info.aggr_attrs.end(),
                      attr) != info.aggr_attrs.end())
            return;

        info.aggr_attrs.push_back(attr);
        info.result_attrs.push_back(make_result_attributes(c, attr));
    }

    void init_aggregation_attributes(Caliper* c) {
        auto attrs = c->find_attributes_with_prop(CALI_ATTR_AGGREGATABLE);

        for (const auto &a : attrs)
            check_aggregation_attribute(c, a);

        const int prop = CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS;
        info.count_attr = c->create_attribute("count", CALI_TYPE_INT, prop);
    }

    void flush_cb(Caliper* c, Channel* chn, SnapshotFlushFn proc_fn) {
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

        size_t num_entries    = 0;
        size_t num_kernels    = 0;
        size_t bytes_reserved = 0;
        size_t num_dropped    = 0;
        size_t max_hash_len   = 0;

        while (tdb) {
            tdb->stopped.store(true);

            num_entries    += tdb->db.num_entries();
            num_kernels    += tdb->db.num_kernels();
            bytes_reserved += tdb->db.bytes_reserved();
            num_dropped    += tdb->db.num_dropped();
            max_hash_len    = std::max(max_hash_len, tdb->db.max_hash_len());

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

            Log(2).stream() << chn->name()  << ": Aggregate: Releasing aggregation DB.\n"
                            << "  max hash len: "
                            << max_hash_len << ", "
                            << num_entries  << " entries, "
                            << num_kernels  << " kernels, "
                            << bytes_reserved_fmt.val    << " "
                            << bytes_reserved_fmt.symbol << " reserved."
                            << std::endl;
        }

        if (num_dropped > 0)
            Log(1).stream() << chn->name() << ": Aggregate: " << num_dropped
                            << " entries dropped because aggregation buffers are full!"
                            << std::endl;
    }

    void process_snapshot_cb(Caliper* c, Channel* chn, SnapshotView rec) {
        ThreadDB* tdb = acquire_tdb(c, chn, !c->is_signal());

        if (tdb && !tdb->stopped.load())
            tdb->db.process_snapshot(c, rec, info);
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
        }
    }

    void post_init_cb(Caliper* c, Channel* chn) {
        // Update key attributes
        for (const Attribute& a : c->get_all_attributes())
            check_key_attribute(a);

        init_aggregation_attributes(c);

        // Initialize master-thread aggregation DB
        acquire_tdb(c, chn, true);
    }

    void create_attribute_cb(Caliper* c, const Attribute& attr) {
        check_key_attribute(attr);
        check_aggregation_attribute(c, attr);
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
            info.implicit_grouping = true;
            return;
        }

        info.implicit_grouping = false;

        // Check for "*" and "path" special arguments to determine
        // nested or implicit grouping

        {
            auto it = std::find(key_attribute_names.begin(), key_attribute_names.end(), "*");

            if (it != key_attribute_names.end()) {
                info.implicit_grouping = true;
                key_attribute_names.erase(it);
            }
        }

        {
            auto it = std::find(key_attribute_names.begin(), key_attribute_names.end(), "path");
            if (it != key_attribute_names.end()) {
                info.group_nested = true;
                key_attribute_names.erase(it);
            }

            it = std::find(key_attribute_names.begin(), key_attribute_names.end(), "prop:nested");
            if (it != key_attribute_names.end()) {
                info.group_nested = true;
                key_attribute_names.erase(it);
            }
        }
    }

    Aggregate(Caliper* c, Channel* chn)
        : num_dropped_snapshots(0)
        {
            config = services::init_config_from_spec(chn->config(), s_spec);

            tdb_lock.unlock();

            info.implicit_grouping = true;
            info.group_nested = false;

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

    static const char* s_spec;

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
            [instance](Caliper* c, Channel* chn, SnapshotView, SnapshotView rec){
                instance->process_snapshot_cb(c, chn, rec);
            });
        chn->events().flush_evt.connect(
            [instance](Caliper* c, Channel* chn, SnapshotView, SnapshotFlushFn proc_fn){
                instance->flush_cb(c, chn, proc_fn);
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

const char* Aggregate::s_spec = R"json(
{   "name"        : "aggregate",
    "description" : "Aggregate snapshots at runtime",
    "config" : [
      { "name"        : "key",
        "description" : "Attributes in the aggregation key (i.e., group by)",
        "type"        : "string"
      }
    ]
}
)json";

} // namespace [anonymous]

namespace cali
{

CaliperService aggregate_service { ::Aggregate::s_spec, ::Aggregate::aggregate_register };

}
