// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "util.h"

#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

#include "caliper/common/Log.h"

#include "../../common/StringConverter.h"

#include "../../services/Services.h"

#include <algorithm>

using namespace cali;

namespace
{

class RuntimeReportController : public cali::ChannelController
{
public:

    RuntimeReportController(
        bool                          use_mpi,
        const char*                   name,
        const config_map_t&           initial_cfg,
        const ConfigManager::Options& opts
    )
        : ChannelController(name, 0, initial_cfg)
    {
        //   Scale time. The "where report.l" hack ensures we only process records with
        // a path region entry, in particular for the time percent calculation.
        std::string q_let = " let report.time=scale(sum#time.duration.ns,1e-9),report.l=leaf() where report.l ";

        // Query for first aggregation step in MPI mode (process-local aggregation)
        std::string q_local = " select sum(report.time) group by path " + q_let;
        // Query for serial-mode aggregation
        std::string q_serial =
            " select sum(report.time) as \"Time (E)\""
            ",inclusive_sum(report.time) as \"Time (I)\""
            ",percent_total(report.time) as \"Time % (E)\""
            ",inclusive_percent_total(report.time) as \"Time % (I)\""
            " group by path " + q_let;

        std::string tmetric = "sum#report.time";
        std::string pmetric = "percent_total(sum#report.time)";

        if (opts.is_enabled("calc.inclusive")) {
            q_local.append(" select inclusive_sum(report.time) ");
            tmetric = "inclusive#report.time";
            pmetric = "inclusive_percent_total(sum#report.time)";
        }

        // Config for second aggregation step in MPI mode (cross-process aggregation)
        std::string q_cross = std::string(" select min(") + tmetric + ") as \"Min time/rank\"" + std::string(",max(")
                                   + tmetric + ") as \"Max time/rank\"" + std::string(",avg(") + tmetric
                                   + ") as \"Avg time/rank\"" + std::string(",") + pmetric + " as \"Time %\" ";

        std::string format = std::string(" format ") + util::build_tree_format_spec(config(), opts);

        q_serial.append(format);
        q_cross.append(format);

        if (use_mpi) {
            config()["CALI_SERVICES_ENABLE"].append(",mpi,mpireport");
            config()["CALI_MPIREPORT_FILENAME"]          = opts.get("output", "stderr");
            config()["CALI_MPIREPORT_APPEND"]            = opts.get("output.append");
            config()["CALI_MPIREPORT_WRITE_ON_FINALIZE"] = "false";
            config()["CALI_MPIREPORT_LOCAL_CONFIG"]      = opts.build_query("local", q_local);
            config()["CALI_MPIREPORT_CONFIG"]            = opts.build_query("cross", q_cross);
        } else {
            config()["CALI_SERVICES_ENABLE"].append(",report");
            config()["CALI_REPORT_FILENAME"] = opts.get("output", "stderr");
            config()["CALI_REPORT_APPEND"]   = opts.get("output.append");
            config()["CALI_REPORT_CONFIG"]   = opts.build_query("local", q_serial);
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
        Log(0).stream() << "runtime-report: cannot enable mpi support: mpireport service is not available."
                        << std::endl;
    }

    return use_mpi;
}

cali::ChannelController* make_runtime_report_controller(
    const char*                         name,
    const config_map_t&                 initial_cfg,
    const cali::ConfigManager::Options& opts
)
{
    return new RuntimeReportController(use_mpi(opts), name, initial_cfg, opts);
}

const char* runtime_report_spec = R"json(
{
 "name"        : "runtime-report",
 "description" : "Print a time profile for annotated regions",
 "categories"  : [ "metric", "output", "region", "treeformatter", "event" ],
 "services"    : [ "aggregate", "event", "timer" ],
 "config"      :
 {
  "CALI_CHANNEL_FLUSH_ON_EXIT": "false",
  "CALI_EVENT_ENABLE_SNAPSHOT_INFO": "false",
  "CALI_TIMER_UNIT": "sec"
 },
 "defaults"    : { "order_as_visited": "true", "output.append": "true" },
 "options"     :
 [
  {
   "name": "calc.inclusive",
   "type": "bool",
   "description": "Report inclusive instead of exclusive times"
  },{
   "name": "aggregate_across_ranks",
   "type": "bool",
   "description": "Aggregate results across MPI ranks"
  },{
   "name": "order_by_time",
   "type": "bool",
   "description": "Order tree branches by highest exclusive runtime",
   "query":
   {
    "local": "order by sum#report.time desc",
    "cross": "aggregate sum(sum#report.time) order by sum#sum#report.time desc"
   }
  },{
   "name": "output.append",
   "type": "bool",
   "description": "Use append mode when writing to files"
  }
 ]
}
)json";

} // namespace

namespace cali
{

ConfigManager::ConfigInfo runtime_report_controller_info { ::runtime_report_spec,
                                                           ::make_runtime_report_controller,
                                                           nullptr };

}
