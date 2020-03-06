// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

#include "caliper/common/Log.h"

#include "../../services/Services.h"

#include <algorithm>

using namespace cali;

namespace
{

class CudaActivityController : public cali::ChannelController
{
public:

    CudaActivityController(bool use_mpi, const ConfigManager::Options& opts)
        : ChannelController("cuda-activity", 0, {
                { "CALI_CHANNEL_FLUSH_ON_EXIT",      "false"  },
                { "CALI_SERVICES_ENABLE", "cuptitrace,aggregate,event" },
                { "CALI_EVENT_ENABLE_SNAPSHOT_INFO", "false"  },
                { "CALI_CUPTITRACE_SNAPSHOT_DURATION", "true" }
            })
        {
            // Config for first aggregation step in MPI mode (process-local aggregation)
            std::string local_select =
                " inclusive_scale(sum#cupti.host.duration,1e-9)"
                ",inclusive_scale(cupti.activity.duration,1e-9)";
            // Config for serial-mode aggregation
            std::string serial_select =
                " inclusive_scale(sum#cupti.host.duration,1e-9) as \"Host Time\""
                ",inclusive_scale(cupti.activity.duration,1e-9) as \"GPU Time\""
                ",ratio(cupti.activity.duration,sum#cupti.host.duration,100.0) as \"GPU %\"";

            // Config for second aggregation step in MPI mode (cross-process aggregation)
            std::string cross_select =
                " avg(iscale#sum#cupti.host.duration) as \"Avg Host Time\""
                ",max(iscale#sum#cupti.host.duration) as \"Max Host Time\""
                ",avg(iscale#cupti.activity.duration) as \"Avg GPU Time\""
                ",max(iscale#cupti.activity.duration) as \"Max GPU Time\""
                ",ratio(iscale#cupti.activity.duration,iscale#sum#cupti.host.duration,100.0) as \"GPU %\"";

            if (use_mpi) {
                config()["CALI_SERVICES_ENABLE"   ].append(",mpi,mpireport");
                config()["CALI_MPIREPORT_FILENAME"] = opts.get("output", "stderr").to_string();
                config()["CALI_MPIREPORT_WRITE_ON_FINALIZE"] = "false";
                config()["CALI_MPIREPORT_LOCAL_CONFIG"] =
                    std::string("select ")
                    + opts.query_select("local", local_select)
                    + " group by "
                    + opts.query_groupby("local", "prop:nested");
                config()["CALI_MPIREPORT_CONFIG"  ] =
                    std::string("select ")
                    + opts.query_select("cross", cross_select)
                    + " group by "
                    + opts.query_groupby("cross", "prop:nested")
                    + " format tree";
            } else {
                config()["CALI_SERVICES_ENABLE"   ].append(",report");
                config()["CALI_REPORT_FILENAME"   ] = opts.get("output", "stderr").to_string();
                config()["CALI_REPORT_CONFIG"     ] =
                    std::string("select ")
                    + opts.query_select("serial", serial_select)
                    + " group by "
                    + opts.query_groupby("serial", "prop:nested")
                    + " format tree";
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
        Log(0).stream() << "cuda-activity: cannot enable mpi support: mpireport service is not available."
                        << std::endl;
    }

    return use_mpi;
}

cali::ChannelController*
make_controller(const cali::ConfigManager::Options& opts)
{
    return new CudaActivityController(use_mpi(opts), opts);
}

const char* controller_spec =
    "{"
    " \"name\"        : \"cuda-activity\","
    " \"description\" : \"Record and print CUDA activities (kernel executions, memcopies, etc.)\","
    " \"categories\"  : [ \"output\", \"region\" ],"
    " \"services\"    : [ \"cupti\", \"cuptitrace\" ],"
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

ConfigManager::ConfigInfo cuda_activity_controller_info
{
    ::controller_spec, ::make_controller, nullptr
};

}
