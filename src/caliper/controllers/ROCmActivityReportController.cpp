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

class RocmActivityReportController : public cali::ChannelController
{
public:

    RocmActivityReportController(
        bool                          use_mpi,
        const char*                   name,
        const config_map_t&           initial_cfg,
        const ConfigManager::Options& opts
    )
        : ChannelController(name, 0, initial_cfg)
    {
        // Config for local aggregation
        std::string local_select =
            " inclusive_scale(sum#time.duration.ns,1e-9) as \"Host Time\""
            ",inclusive_scale(sum#rocm.activity.duration,1e-9) as \"GPU Time\""
            ",inclusive_ratio(sum#rocm.activity.duration,sum#time.duration.ns,100.0) as \"GPU %\"";

        // Config for second aggregation step in MPI mode (cross-process aggregation)
        std::string cross_select =
            " avg(iscale#sum#time.duration.ns) as \"Avg Host Time\""
            ",max(iscale#sum#time.duration.ns) as \"Max Host Time\""
            ",avg(iscale#sum#rocm.activity.duration) as \"Avg GPU Time\""
            ",max(iscale#sum#rocm.activity.duration) as \"Max GPU Time\""
            ",ratio(iscale#sum#rocm.activity.duration,iscale#sum#time.duration.ns,100.0) as \"GPU %\"";

        std::string groupby = "path";

        if (opts.is_enabled("show_kernels")) {
            groupby += ",rocm.kernel.name";
            local_select = std::string("rocm.kernel.name as Kernel,") + local_select;
            cross_select  = std::string("rocm.kernel.name as Kernel,") + cross_select;
        }

        std::string format = util::build_tree_format_spec(config(), opts);

        std::string local_query = "select ";
        local_query.append(local_select);
        local_query.append(" group by ").append(groupby);
        local_query.append(" format ").append(format);

        std::string cross_query = "select ";
        cross_query.append(cross_select);
        cross_query.append(" group by ").append(groupby);
        cross_query.append(" format ").append(format);

        if (use_mpi) {
            config()["CALI_SERVICES_ENABLE"].append(",mpi,mpireport");
            config()["CALI_MPIREPORT_FILENAME"]          = opts.get("output", "stderr");
            config()["CALI_MPIREPORT_APPEND"]            = opts.get("output.append");
            config()["CALI_MPIREPORT_WRITE_ON_FINALIZE"] = "false";
            config()["CALI_MPIREPORT_LOCAL_CONFIG"]      = opts.build_query("local", local_query);
            config()["CALI_MPIREPORT_CONFIG"]            = opts.build_query("cross", cross_query);
        } else {
            config()["CALI_SERVICES_ENABLE"].append(",report");
            config()["CALI_REPORT_FILENAME"] = opts.get("output", "stderr");
            config()["CALI_REPORT_APPEND"]   = opts.get("output.append");
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
        Log(0).stream() << "rocm-activity: cannot enable mpi support: mpireport service is not available." << std::endl;
    }

    return use_mpi;
}

cali::ChannelController* make_controller(
    const char*                         name,
    const config_map_t&                 initial_cfg,
    const cali::ConfigManager::Options& opts
)
{
    return new RocmActivityReportController(use_mpi(opts), name, initial_cfg, opts);
}

const char* controller_spec = R"json(
{
 "name"        : "rocm-activity-report",
 "description" : "Record and print AMD ROCm activities (kernel executions, memcopies, etc.)",
 "categories"  : [ "output", "region", "treeformatter", "event" ],
 "services"    : [ "aggregate", "roctracer", "event", "timer" ],
 "config"      :
 {
  "CALI_CHANNEL_FLUSH_ON_EXIT"       : "false",
  "CALI_EVENT_ENABLE_SNAPSHOT_INFO"  : "false",
  "CALI_ROCTRACER_TRACE_ACTIVITIES"  : "true"
 },
 "defaults"    : { "order_as_visited": "true", "output.append": "true" },
 "options":
 [
  {
   "name": "aggregate_across_ranks",
   "type": "bool",
   "description": "Aggregate results across MPI ranks"
  },{
   "name": "show_kernels",
   "type": "bool",
   "config": { "CALI_ROCTRACER_RECORD_KERNEL_NAMES": "true" },
   "description": "Show kernel names"
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

ConfigManager::ConfigInfo rocm_activity_report_controller_info { ::controller_spec, ::make_controller, nullptr };

}
