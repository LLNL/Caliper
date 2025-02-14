// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

#include "caliper/common/Log.h"
#include "caliper/common/StringConverter.h"

#include "../../services/Services.h"

#include <algorithm>

using namespace cali;

namespace
{

class OpenMPReportController : public cali::ChannelController
{
public:

    OpenMPReportController(
        bool                          use_mpi,
        const char*                   name,
        const config_map_t&           initial_cfg,
        const ConfigManager::Options& opts
    )
        : ChannelController(name, 0, initial_cfg)
    {
        // Config for serial-mode aggregation
        std::string local_query =
            "let sum#time.duration=scale(sum#time.duration.ns,1e-9)"
            ",n.threads=first(omp.num.threads)"
            ",t.initial=first(sum#time.duration) if omp.thread.type=initial"
            " select max(n.threads) as \"#Threads\""
            ",inclusive_sum(t.initial) as \"Time (thread)\""
            ",inclusive_sum(sum#time.duration) as \"Time (total)\"";

        // Config for second aggregation step in MPI mode (cross-process aggregation)
        std::string cross_query =
            " max(max#n.threads) as \"#Threads\""
            ",avg(inclusive#t.initial) as \"Time (thread) (avg)\""
            ",sum(inclusive#sum#time.duration) as \"Time (total)\"";

        const char* format = opts.is_enabled("show_regions") ? "tree" : "table";

        local_query.append(" format ").append(format);
        cross_query.append(" format ").append(format);

        if (use_mpi) {
            config()["CALI_SERVICES_ENABLE"].append(",mpi,mpireport");
            config()["CALI_MPIREPORT_FILENAME"]          = opts.get("output", "stderr");
            config()["CALI_MPIREPORT_WRITE_ON_FINALIZE"] = "false";
            config()["CALI_MPIREPORT_LOCAL_CONFIG"]      = opts.build_query("local", local_query);
            config()["CALI_MPIREPORT_CONFIG"]            = opts.build_query("cross", cross_query);
        } else {
            config()["CALI_SERVICES_ENABLE"].append(",report");
            config()["CALI_REPORT_FILENAME"] = opts.get("output", "stderr");
            config()["CALI_REPORT_CONFIG"]   = opts.build_query("local", local_query);
        }

        opts.update_channel_config(config());
        opts.update_channel_metadata(metadata());
    }
};

// Parse the "mpi=" argument
bool use_mpi(const cali::ConfigManager::Options& opts)
{
    auto services = services::get_available_services();

    bool have_mpireport = std::find(services.begin(), services.end(), "mpireport") != services.end();

    bool use_mpi = have_mpireport;

    if (opts.is_set("aggregate_across_ranks"))
        use_mpi = StringConverter(opts.get("aggregate_across_ranks")).to_bool();

    if (use_mpi && !have_mpireport) {
        use_mpi = false;
        Log(0).stream() << "openmp-report: cannot enable mpi support: mpireport service is not available." << std::endl;
    }

    return use_mpi;
}

cali::ChannelController* make_controller(
    const char*                         name,
    const config_map_t&                 initial_cfg,
    const cali::ConfigManager::Options& opts
)
{
    return new OpenMPReportController(use_mpi(opts), name, initial_cfg, opts);
}

const char* controller_spec = R"json(
{
 "name"        : "openmp-report",
 "description" : "Record and print OpenMP performance metrics (loops, barriers, etc.)",
 "categories"  : [ "output", "region", "metric", "event" ],
 "services"    : [ "aggregate", "ompt", "event" ],
 "config"      : 
 { "CALI_CHANNEL_FLUSH_ON_EXIT": "false",
   "CALI_EVENT_ENABLE_SNAPSHOT_INFO": "false"
 },
 "defaults"    : 
 { "openmp.times"      : "true",
   "openmp.efficiency" : "true",
   "show_regions"      : "true"
 },
 "options": 
 [
  {
   "name": "aggregate_across_ranks",
   "type": "bool",
   "description": "Aggregate results across MPI ranks"
  },
  {
   "name": "show_threads",
   "type": "bool",
   "description": "Show thread IDs",
   "query":
   {
    "local": "select omp.thread.id as Thread group by omp.thread.id",
    "cross": "select omp.thread.id as Thread group by omp.thread.id"
   }
  },
  {
   "name": "show_thread_type",
   "type": "bool",
   "description": "Show thread type (initial vs. worker)",
   "query":
   {
    "local": "select omp.thread.type as Type group by omp.thread.type",
    "cross": "select omp.thread.type as Type group by omp.thread.type"
   }
  },
  {
   "name": "show_regions",
   "type": "bool",
   "description": "Show Caliper region tree",
   "query": { "local": "group by path", "cross": "group by path" }
  }
 ]
};
)json";

} // namespace

namespace cali
{

ConfigManager::ConfigInfo openmp_report_controller_info { ::controller_spec, ::make_controller, nullptr };

}
