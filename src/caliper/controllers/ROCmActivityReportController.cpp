// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "util.h"

#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

#include "caliper/common/Log.h"

#include "../../services/Services.h"

#include <algorithm>

using namespace cali;

namespace
{

class RocmActivityReportController : public cali::ChannelController
{
public:

    RocmActivityReportController(bool use_mpi, const char* name, const config_map_t& initial_cfg, const ConfigManager::Options& opts)
        : ChannelController(name, 0, initial_cfg)
        {
            // Config for first aggregation step in MPI mode (process-local aggregation)
            std::string local_select =
                " inclusive_sum(sum#time.duration)"
                ",inclusive_scale(sum#rocm.activity.duration,1e-9)";
            // Config for serial-mode aggregation
            std::string serial_select =
                " inclusive_sum(sum#time.duration) as \"Host Time\""
                ",inclusive_scale(sum#rocm.activity.duration,1e-9) as \"GPU Time\""
                ",inclusive_ratio(sum#rocm.activity.duration,sum#time.duration,1e-7) as \"GPU %\"";

            // Config for second aggregation step in MPI mode (cross-process aggregation)
            std::string cross_select =
                " avg(inclusive#sum#time.duration) as \"Avg Host Time\""
                ",max(inclusive#sum#time.duration) as \"Max Host Time\""
                ",avg(iscale#sum#rocm.activity.duration) as \"Avg GPU Time\""
                ",max(iscale#sum#rocm.activity.duration) as \"Max GPU Time\""
                ",ratio(iscale#sum#rocm.activity.duration,inclusive#sum#time.duration,100.0) as \"GPU %\"";

            std::string groupby = "path";

            if (opts.is_enabled("show_kernels")) {
                groupby += ",rocm.kernel.name";
                serial_select
                    = std::string("rocm.kernel.name as Kernel,") + serial_select;
                cross_select
                    = std::string("rocm.kernel.name as Kernel,") + cross_select;
            }

            std::string format = util::build_tree_format_spec(config(), opts);

            if (use_mpi) {
                config()["CALI_SERVICES_ENABLE"   ].append(",mpi,mpireport");
                config()["CALI_MPIREPORT_FILENAME"] = opts.get("output", "stderr").to_string();
                config()["CALI_MPIREPORT_WRITE_ON_FINALIZE"] = "false";
                config()["CALI_MPIREPORT_LOCAL_CONFIG"] =
                    opts.build_query("local", {
                            { "select",   local_select },
                            { "group by", groupby }
                        });
                config()["CALI_MPIREPORT_CONFIG"  ] =
                    opts.build_query("cross", {
                            { "select",   cross_select  },
                            { "group by", groupby },
                            { "format",   format  }
                        });
            } else {
                config()["CALI_SERVICES_ENABLE"   ].append(",report");
                config()["CALI_REPORT_FILENAME"   ] = opts.get("output", "stderr").to_string();
                config()["CALI_REPORT_CONFIG"     ] =
                    opts.build_query("local", {
                            { "select",   serial_select },
                            { "group by", groupby },
                            { "format",   format  }
                        });
            }

            opts.update_channel_config(config());
        }
};

// Parse the "mpi=" argument
bool
use_mpi(const cali::ConfigManager::Options& opts)
{
    auto services = services::get_available_services();

    bool have_mpireport =
        std::find(services.begin(), services.end(), "mpireport") != services.end();

    bool use_mpi = have_mpireport;

    if (opts.is_set("aggregate_across_ranks"))
        use_mpi = opts.get("aggregate_across_ranks").to_bool();

    if (use_mpi && !have_mpireport) {
        use_mpi = false;
        Log(0).stream() << "rocm-activity: cannot enable mpi support: mpireport service is not available."
                        << std::endl;
    }

    return use_mpi;
}

cali::ChannelController*
make_controller(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts)
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
       { "CALI_CHANNEL_FLUSH_ON_EXIT"       : "false",
         "CALI_EVENT_ENABLE_SNAPSHOT_INFO"  : "false",
         "CALI_ROCTRACER_TRACE_ACTIVITIES"  : "true"
       },
     "options":
     [
      {
       "name": "aggregate_across_ranks",
       "type": "bool",
       "description": "Aggregate results across MPI ranks"
      },
      {
       "name": "show_kernels",
       "type": "bool",
       "config": { "CALI_ROCTRACER_RECORD_KERNEL_NAMES": "true" },
       "description": "Show kernel names"
      }
     ]
    }
)json";

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo rocm_activity_report_controller_info
{
    ::controller_spec, ::make_controller, nullptr
};

}
