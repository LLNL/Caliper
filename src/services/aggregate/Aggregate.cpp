// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Caliper on-line aggregation service

#include "AggregationDB.h"

#include "../Services.h"
#include "../../common/util/unitfmt.h"
#include "../../common/util/spinlock.hpp"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Variant.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <sstream>

using namespace aggregate;
using namespace cali;

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

        ThreadDB* next = nullptr;
        ThreadDB* prev = nullptr;

        AggregationDB db;

        void unlink()
        {
            if (next)
                next->prev = prev;
            if (prev)
                prev->next = next;
        }

        ThreadDB(Caliper* c) : stopped(false), retired(false), db(c) {}
    };

    std::string m_channel_name;

    ThreadDB*      m_tdb_list = nullptr;
    util::spinlock m_tdb_lock;

    AttributeInfo            m_attr_info;
    std::vector<std::string> m_key_attribute_names;

    Attribute m_tdb_attr;

    size_t m_num_dropped_snapshots;

    inline ThreadDB* acquire_tdb(Caliper* c, bool can_alloc)
    {
        //   we store a pointer to the thread-local aggregation DB for this channel
        // on the thread's blackboard

        ThreadDB* tdb = static_cast<ThreadDB*>(c->get(m_tdb_attr).value().get_ptr());

        if (!tdb && can_alloc) {
            tdb = new ThreadDB(c);

            c->set(m_tdb_attr, Variant(cali_make_variant_from_ptr(tdb)));

            std::lock_guard<util::spinlock> g(m_tdb_lock);

            if (m_tdb_list)
                m_tdb_list->prev = tdb;

            tdb->next  = m_tdb_list;
            m_tdb_list = tdb;
        }

        return tdb;
    }

    ResultAttributes make_result_attributes(Caliper* c, const Attribute& attr)
    {
        std::string      name = attr.name();
        ResultAttributes res;

        int            prop = CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS;
        cali_attr_type type = attr.type();

        res.min_attr = c->create_attribute(std::string("min#") + name, type, prop);
        res.max_attr = c->create_attribute(std::string("max#") + name, type, prop);
        res.sum_attr = c->create_attribute(std::string("sum#") + name, type, prop);
        res.avg_attr = c->create_attribute(std::string("avg#") + name, type, prop);
#ifdef CALIPER_ENABLE_HISTOGRAMS
        for (int jj = 0; jj < CALI_AGG_HISTOGRAM_BINS; jj++) {
            res.histogram_attr[jj] = c->create_attribute(
                std::string("histogram.bin.") + std::to_string(jj) + std::string("#") + name,
                CALI_TYPE_INT,
                prop
            );
        }
#endif

        return res;
    }

    void check_aggregation_attribute(Caliper* c, const Attribute& attr)
    {
        if (!(attr.properties() & CALI_ATTR_AGGREGATABLE))
            return;

        if (std::find(m_attr_info.aggr_attrs.begin(), m_attr_info.aggr_attrs.end(), attr) != m_attr_info.aggr_attrs.end())
            return;

        m_attr_info.aggr_attrs.push_back(attr);
        m_attr_info.result_attrs.push_back(make_result_attributes(c, attr));
    }

    void init_aggregation_attributes(Caliper* c)
    {
        auto attrs = c->find_attributes_with_prop(CALI_ATTR_AGGREGATABLE);

        for (const auto& a : attrs)
            check_aggregation_attribute(c, a);

        const int prop = CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS;

        m_attr_info.count_attr = c->create_attribute("count", CALI_TYPE_UINT, prop);
        m_attr_info.slot_attr  = c->create_attribute("aggregate.slot", CALI_TYPE_UINT, prop);
    }

    void flush_cb(Caliper* c, SnapshotFlushFn proc_fn)
    {
        ThreadDB* tdb = nullptr;

        {
            std::lock_guard<util::spinlock> g(m_tdb_lock);
            tdb = m_tdb_list;
        }

        size_t num_written = 0;

        for (; tdb; tdb = tdb->next) {
            tdb->stopped.store(true);
            num_written += tdb->db.flush(m_attr_info, c, proc_fn);
            tdb->stopped.store(false);
        }

        Log(1).stream() << m_channel_name << ": Aggregate: flushed " << num_written << " snapshots." << std::endl;
    }

    void clear_cb(Caliper* c, Channel* chn)
    {
        ThreadDB* tdb = nullptr;

        {
            std::lock_guard<util::spinlock> g(m_tdb_lock);
            tdb = m_tdb_list;
        }

        size_t num_entries    = 0;
        size_t num_kernels    = 0;
        size_t bytes_reserved = 0;
        size_t num_dropped    = 0;
        size_t max_hash_len   = 0;

        while (tdb) {
            tdb->stopped.store(true);

            num_entries += tdb->db.num_entries();
            num_kernels += tdb->db.num_kernels();
            bytes_reserved += tdb->db.bytes_reserved();
            num_dropped += tdb->db.num_dropped();
            max_hash_len = std::max(max_hash_len, tdb->db.max_hash_len());
            tdb->db.clear();

            tdb->stopped.store(false);

            if (tdb->retired) {
                ThreadDB* tmp = tdb->next;

                {
                    std::lock_guard<util::spinlock> g(m_tdb_lock);

                    tdb->unlink();
                    if (tdb == m_tdb_list)
                        m_tdb_list = tmp;
                }

                delete tdb;
                tdb = tmp;
            } else {
                tdb = tdb->next;
            }
        }

        if (Log::verbosity() >= 2) {
            unitfmt_result bytes_reserved_fmt = unitfmt(bytes_reserved, unitfmt_bytes);

            Log(2).stream() << chn->name() << ": Aggregate: Releasing aggregation DB.\n"
                            << "  max hash len: " << max_hash_len << ", " << num_entries << " entries, " << num_kernels
                            << " kernels, " << bytes_reserved_fmt.val << " " << bytes_reserved_fmt.symbol
                            << " reserved." << std::endl;
        }

        if (num_dropped > 0)
            Log(1).stream() << chn->name() << ": Aggregate: " << num_dropped
                            << " entries dropped because aggregation buffers are full!" << std::endl;
    }

    void process_snapshot_cb(Caliper* c, SnapshotView rec)
    {
        ThreadDB* tdb = acquire_tdb(c, !c->is_signal());

        if (tdb && !tdb->stopped.load())
            tdb->db.process_snapshot(c, rec, m_attr_info);
        else
            ++m_num_dropped_snapshots;
    }

    void check_key_attribute(const Attribute& attr)
    {
        auto it = std::find(m_key_attribute_names.begin(), m_key_attribute_names.end(), attr.name());

        if (it != m_key_attribute_names.end()) {
            if (attr.store_as_value()) {
                m_attr_info.imm_key_attrs.push_back(attr);
            } else {
                Log(1).stream() << m_channel_name
                    << ": aggregate: Reference attributes are no longer supported in CALI_AGGREGATE_KEY, ignoring "
                    << attr.name() << std::endl;
            }

            m_key_attribute_names.erase(it);
        }
    }

    void post_init_cb(Caliper* c, Channel* chn)
    {
        // Update key attributes
        for (const Attribute& a : c->get_all_attributes())
            check_key_attribute(a);

        init_aggregation_attributes(c);

        // Initialize master-thread aggregation DB
        acquire_tdb(c, true);
    }

    void create_attribute_cb(Caliper* c, const Attribute& attr)
    {
        check_key_attribute(attr);
        check_aggregation_attribute(c, attr);
    }

    void create_thread_cb(Caliper* c) 
    {
        acquire_tdb(c, true); 
    }

    void release_thread_cb(Caliper* c)
    {
        ThreadDB* tdb = acquire_tdb(c, false);

        if (tdb)
            tdb->retired.store(true);
    }

    void finish_cb(Caliper* c, Channel* chn)
    {
        if (m_num_dropped_snapshots > 0)
            Log(1).stream() << m_channel_name << ": Aggregate: dropped " << m_num_dropped_snapshots << " snapshots."
                            << std::endl;
    }

    Aggregate(Caliper* c, Channel* chn) : m_channel_name { chn->name() }, m_num_dropped_snapshots(0)
    {
        auto cfg = services::init_config_from_spec(chn->config(), s_spec);        
        m_key_attribute_names = cfg.get("key").to_stringlist(",");
        
        m_tdb_attr = c->create_attribute(
            std::string("aggregate.tdb.") + std::to_string(chn->id()),
            CALI_TYPE_PTR,
            CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_HIDDEN
        );

        m_tdb_lock.unlock();
    }

public:

    static const char* s_spec;

    ~Aggregate()
    {
        ThreadDB* tdb = m_tdb_list;

        while (tdb) {
            ThreadDB* tmp = tdb->next;

            tdb->unlink();
            if (tdb == m_tdb_list)
                m_tdb_list = tmp;

            delete tdb;
            tdb = tmp;
        }
    }

    static void aggregate_register(Caliper* c, Channel* chn)
    {
        Aggregate* instance = new Aggregate(c, chn);

        chn->events().create_attr_evt.connect([instance](Caliper* c, const Attribute& attr) {
            instance->create_attribute_cb(c, attr);
        });
        chn->events().post_init_evt.connect([instance](Caliper* c, Channel* chn) { instance->post_init_cb(c, chn); });
        chn->events().create_thread_evt.connect([instance](Caliper* c, Channel* chn) {
            instance->create_thread_cb(c);
        });
        chn->events().release_thread_evt.connect([instance](Caliper* c, Channel* chn) {
            instance->release_thread_cb(c);
        });
        chn->events().process_snapshot.connect([instance](Caliper* c, SnapshotView, SnapshotView rec) {
            instance->process_snapshot_cb(c, rec);
        });
        chn->events().flush_evt.connect([instance](Caliper* c, SnapshotView, SnapshotFlushFn proc_fn) {
            instance->flush_cb(c, proc_fn);
        });
        chn->events().clear_evt.connect([instance](Caliper* c, Channel* chn) { instance->clear_cb(c, chn); });
        chn->events().finish_evt.connect([instance](Caliper* c, Channel* chn) {
            instance->clear_cb(c, chn); // prints logs
            instance->finish_cb(c, chn);
            delete instance;
        });

        Log(1).stream() << chn->name() << ": Registered aggregation service" << std::endl;
    }

}; // class Aggregate

const char* Aggregate::s_spec = R"json(
{
 "name"        : "aggregate",
 "description" : "Aggregate snapshots at runtime",
 "config" :
 [
  {
   "name"        : "key",
   "description" : "Immediate attributes to include in the aggregation key (group by)",
   "type"        : "string"
  }
 ]
}
)json";

} // namespace

namespace cali
{

CaliperService aggregate_service { ::Aggregate::s_spec, ::Aggregate::aggregate_register };

}
