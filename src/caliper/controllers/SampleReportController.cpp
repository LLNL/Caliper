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

class SampleReportController : public cali::ChannelController
{
public:

    SampleReportController(
        bool                          use_mpi,
        const char*                   name,
        const config_map_t&           initial_cfg,
        const ConfigManager::Options& opts
    )
        : ChannelController(name, 0, initial_cfg)
    {
        double freq = std::max(1.0, std::stod(opts.get("sample.frequency", "200")));

        config()["CALI_SAMPLER_FREQUENCY"] = std::to_string(freq);

        // Config for first aggregation step in MPI mode (process-local aggregation)
        std::string q_local = std::string(" select count() as \"Samples\",scale_count(")
            + std::to_string(1.0 / freq) + ") as \"Time (sec)\" unit sec ";

        // Config for second aggregation step in MPI mode (cross-process aggregation)
        std::string q_cross =
            " select min(scount) as \"Min time/rank\" unit sec"
            ",max(scount) as \"Max time/rank\" unit sec"
            ",avg(scount) as \"Avg time/rank\" unit sec"
            ",sum(scount) as \"Total time\" unit sec"
            ",percent_total(scount) as \"Time %\" ";

        auto avail_services = services::get_available_services();
        bool have_pthread = std::find(avail_services.begin(), avail_services.end(), "pthread") != avail_services.end();

        bool use_callpath = opts.is_enabled("callpath");

        const char* groupby = " group by path ";
        const char* fmt_in  = "";

        if (use_callpath) {
            groupby = " group by source.function#callpath.address ";
            fmt_in  = "path-attributes=source.function#callpath.address";
        }

        std::string fmt_spec = util::build_tree_format_spec(config(), opts, fmt_in);

        q_local.append(groupby);
        q_local.append(" format ").append(fmt_spec);
        q_cross.append(groupby);
        q_cross.append(" format ").append(fmt_spec);

        if (have_pthread)
            config()["CALI_SERVICES_ENABLE"].append(",pthread");

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
            config()["CALI_REPORT_CONFIG"]   = opts.build_query("local", q_local);
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
        Log(0).stream() << "sample-report: cannot enable mpi support: mpireport service is not available." << std::endl;
    }

    return use_mpi;
}

cali::ChannelController* make_sample_report_controller(
    const char*                         name,
    const config_map_t&                 initial_cfg,
    const cali::ConfigManager::Options& opts
)
{
    return new SampleReportController(use_mpi(opts), name, initial_cfg, opts);
}

const char* sample_report_spec = R"json(
{
 "name"        : "sample-report",
 "description" : "Print a sampling profile for the program",
 "categories"  : [ "output", "sampling", "treeformatter", "region" ],
 "services"    : [ "sampler", "trace" ],
 "config"      : { "CALI_CHANNEL_FLUSH_ON_EXIT": "false" },
 "defaults"    : { "source.function": "true", "output.append": "true" },
 "options":
 [
  {
   "name": "sample.frequency",
   "type": "int",
   "description": "Sampling frequency in Hz. Default: 200"
  },{
   "name": "callpath",
   "type": "bool",
   "description": "Group by function call path instead of instrumented region",
   "services": [ "callpath", "symbollookup" ]
  },{
   "name": "aggregate_across_ranks",
   "type": "bool",
   "description": "Aggregate results across MPI ranks"
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

ConfigManager::ConfigInfo sample_report_controller_info { ::sample_report_spec,
                                                          ::make_sample_report_controller,
                                                          nullptr };

}
