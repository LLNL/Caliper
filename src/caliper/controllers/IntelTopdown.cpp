// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include <caliper/Caliper.h>
#include <caliper/ChannelController.h>
#include <caliper/ConfigManager.h>

#include <caliper/reader/Aggregator.h>
#include <caliper/reader/CaliperMetadataDB.h>
#include <caliper/reader/CalQLParser.h>
#include <caliper/reader/FormatProcessor.h>
#include <caliper/reader/NestedExclusiveRegionProfile.h>

#include <caliper/common/Log.h>
#include <caliper/common/OutputStream.h>
#include <caliper/common/StringConverter.h>

#include "../../services/Services.h"

#include <unistd.h>

#include <algorithm>
#include <ctime>

using namespace cali;

namespace
{

Variant get_val_from_rec(const std::vector<Entry>& rec, const Attribute& attr)
{
    Variant ret;
    cali_id_t attr_id = attr.id();

    auto it = std::find_if(rec.begin(), rec.end(), [attr_id](const Entry& e){
            return e.attribute() == attr_id;
        });

    if (it != rec.end())
        ret = it->value();

    return ret;
}

class IntelTopdownController : public cali::ChannelController
{
    ConfigManager::Options m_opts;

    class TopLevelMetrics {
        Attribute cpu_clk_unhalted_thread_p_attr;
        Attribute uops_retired_retire_slots_attr;
        Attribute uops_issued_any_attr;
        Attribute int_misc_recovery_cycles_attr;
        Attribute idq_uops_not_delivered_core_attr;

        std::map<std::string, Attribute> result_attrs;

        SnapshotProcessFn output;

        void make_result_attrs(CaliperMetadataAccessInterface& db) {
            const char* res[] = {
                "retiring", "backend_bound", "frontend_bound", "bad_speculation"
            };

            for (const char* s : res)
                result_attrs[std::string(s)] =
                    db.create_attribute(std::string("topdown.")+s, CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE);
        }

        std::vector<Entry>
        compute(Variant v_cpu_clk_unhalted_thread_p,
                Variant v_uops_retired_retire_slots,
                Variant v_uops_issued_any,
                Variant v_int_misc_recovery_cycles,
                Variant v_idq_uops_not_delivered_core)
        {
            std::vector<Entry> ret;
            ret.reserve(4);

            double clocks = v_cpu_clk_unhalted_thread_p.to_double();

            if (!(clocks > 0.0))
                return ret;

            double slots = 4.0 * clocks;
            double retiring = v_uops_retired_retire_slots.to_double() / slots;
            double bad_speculation = 
                (v_uops_issued_any.to_double() 
                    - v_uops_retired_retire_slots.to_double() 
                    + 4.0 * v_int_misc_recovery_cycles.to_double()) / slots;
            double frontend_bound = v_idq_uops_not_delivered_core.to_double() / slots;
            double backend_bound = 
                1.0 - (retiring + bad_speculation + frontend_bound);

            ret.push_back(Entry(result_attrs["retiring"],     Variant(retiring)));
            ret.push_back(Entry(result_attrs["backend_bound"], Variant(backend_bound)));
            ret.push_back(Entry(result_attrs["frontend_bound"], Variant(frontend_bound)));
            ret.push_back(Entry(result_attrs["bad_speculation"], Variant(bad_speculation)));

            return ret;
        }

    public:

        TopLevelMetrics(CaliperMetadataAccessInterface& db, SnapshotProcessFn fn)
            : output(fn)
            {
                cpu_clk_unhalted_thread_p_attr =
                    db.get_attribute("sum#papi.CPU_CLK_THREAD_UNHALTED:THREAD_P");
                uops_retired_retire_slots_attr =
                    db.get_attribute("sum#papi.UOPS_RETIRED:RETIRE_SLOTS");
                uops_issued_any_attr =
                    db.get_attribute("sum#papi.UOPS_ISSUED:ANY");
                int_misc_recovery_cycles_attr =
                    db.get_attribute("sum#papi.INT_MISC:RECOVERY_CYCLES");
                idq_uops_not_delivered_core_attr = 
                    db.get_attribute("sum#papi.IDQ_UOPS_NOT_DELIVERED:CORE");

                if (cpu_clk_unhalted_thread_p_attr == Attribute::invalid)
                    Log(0).stream() << "CPU_CLK_UNHALTED.THREAD_P counter attribute not found" << std::endl;
                if (uops_retired_retire_slots_attr == Attribute::invalid)
                    Log(0).stream() << "UOPS_RETIRED.RETIRE_SLOTS counter attribute not found" << std::endl;
                if (idq_uops_not_delivered_core_attr == Attribute::invalid)
                    Log(0).stream() << "IDQ_UOPS_NOT_DELIVERED.CORE counter attribute not found" << std::endl;

                make_result_attrs(db);
            }

        void operator()(CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec) {
            std::vector<Entry> res =
                compute(get_val_from_rec(rec, cpu_clk_unhalted_thread_p_attr),
                        get_val_from_rec(rec, uops_retired_retire_slots_attr),
                        get_val_from_rec(rec, uops_issued_any_attr),
                        get_val_from_rec(rec, int_misc_recovery_cycles_attr),
                        get_val_from_rec(rec, idq_uops_not_delivered_core_attr));

            res.insert(res.end(), rec.begin(), rec.end());
            output(db, res);
        }
    };


public:

    void flush() {
        Log(1).stream() << "[intel-topdown]: Flushing data" << std::endl;

        // Sum up local counters

        const char* local_query =
            "group by prop:nested aggregate"
            " sum(papi.CPU_CLK_THREAD_UNHALTED:THREAD_P)"
            ",sum(papi.UOPS_RETIRED:RETIRE_SLOTS)"
            ",sum(papi.UOPS_ISSUED:ANY)"
            ",sum(papi.INT_MISC:RECOVERY_CYCLES)"
            ",sum(papi.IDQ_UOPS_NOT_DELIVERED:CORE)";

        Aggregator agg(CalQLParser(local_query).spec());

        CaliperMetadataDB db;
        Caliper c;

        c.flush(channel(), nullptr, [&db,&agg](CaliperMetadataAccessInterface& in_db, const std::vector<Entry>& rec){
                auto mrec = db.merge_snapshot(in_db, rec);
                agg.add(db, mrec);
            });

        const char* output_query = 
            " select "
            " sum#papi.CPU_CLK_THREAD_UNHALTED:THREAD_P as Clock"
            ",topdown.retiring as Retiring"
            ",topdown.frontend_bound as Frontend"
            ",topdown.backend_bound as Backend"
            ",topdown.bad_speculation as Bad\\ Speculation"
            " format tree";
        
        OutputStream stream;
        stream.set_filename(m_opts.get("output", "stderr").to_string().c_str(), c, c.get_globals());

        FormatProcessor formatter(CalQLParser(output_query).spec(), stream);
        TopLevelMetrics proc(db, formatter);

        agg.flush(db, proc);
        formatter.flush(db);
    }

    IntelTopdownController(bool use_mpi, const cali::ConfigManager::Options& opts)
        : ChannelController("intel-topdown", 0, {
                { "CALI_SERVICES_ENABLE", "event,trace,papi" },
                { "CALI_CHANNEL_FLUSH_ON_EXIT",  "false" },
                { "CALI_CHANNEL_CONFIG_CHECK",   "false" },
                { "CALI_EVENT_ENABLE_SNAPSHOT_INFO", "false" },
                { "CALI_LIBPFM_ENABLE_SAMPLING", "false" },
                { "CALI_LIBPFM_RECORD_COUNTERS", "true"  },
                { "CALI_PAPI_COUNTERS",
                    "CPU_CLK_THREAD_UNHALTED:THREAD_P,"
                    "UOPS_RETIRED:RETIRE_SLOTS,"
                    "UOPS_ISSUED:ANY,"
                    "INT_MISC:RECOVERY_CYCLES,"
                    "IDQ_UOPS_NOT_DELIVERED:CORE"
                }
            }),
            m_opts(opts)
        {
            // config()["CALI_SAMPLER_FREQUENCY"] = m_opts.get("sampler.frequency", "100");

            m_opts.update_channel_config(config());
        }
};


cali::ChannelController*
make_controller(const cali::ConfigManager::Options& opts) {
    bool use_mpi = false;

    if (opts.is_set("aggregate_across_ranks"))
        use_mpi = opts.get("aggregate_across_ranks").to_bool();

    return new IntelTopdownController(use_mpi, opts);
}

const char* controller_spec =
    "{"
    " \"name\"        : \"intel-topdown\","
    " \"description\" : \"Perform top-down CPU bottleneck analysis for Intel Skylake\","
    " \"categories\"  : [ \"output\", \"region\" ],"
    " \"services\"    : [ \"papi\" ],"
    " \"options\": "
    " ["
    "  {"
    "   \"name\": \"aggregate_across_ranks\","
    "   \"type\": \"bool\","
    "   \"description\": \"Aggregate results across MPI ranks\""
    "  }"
    " ]"
    "}";

} // namespace [anonymous]


namespace cali
{

ConfigManager::ConfigInfo intel_topdown_controller_info
{
    ::controller_spec, ::make_controller, nullptr
};

}
