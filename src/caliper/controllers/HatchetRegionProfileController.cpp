// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

#include "caliper/common/Log.h"

#include "../../services/Services.h"

#include <algorithm>
#include <set>
#include <tuple>

using namespace cali;

namespace
{

class HatchetRegionProfileController : public cali::ChannelController
{
public:

    HatchetRegionProfileController(
        const char*                   name,
        const config_map_t&           initial_cfg,
        const ConfigManager::Options& opts,
        const std::string&            format_spec
    )
        : ChannelController(name, 0, initial_cfg)
    {
        std::string output(opts.get("output", "region_profile"));

        if (output != "stdout" && output != "stderr") {
            auto        pos = output.find_last_of('.');
            std::string ext = (format_spec == "cali" ? ".cali" : ".json");

            if (pos == std::string::npos || output.substr(pos) != ext)
                output.append(ext);
        }

        auto avail_services = services::get_available_services();
        bool have_mpi = std::find(avail_services.begin(), avail_services.end(), "mpireport") != avail_services.end();
        bool have_adiak =
            std::find(avail_services.begin(), avail_services.end(), "adiak_import") != avail_services.end();

        bool use_mpi = have_mpi;

        if (opts.is_set("use.mpi"))
            use_mpi = have_mpi && opts.is_enabled("use.mpi");

        if (have_adiak) {
            config()["CALI_SERVICES_ENABLE"].append(",adiak_import");
            config()["CALI_ADIAK_IMPORT_CATEGORIES"] = opts.get("adiak.import_categories", "2,3");
        }

        std::string query =
            " let sum#time.duration=scale(sum#time.duration.ns,1e-9) "
            " select *,sum(sum#time.duration) as time unit sec "
            " group by path,mpi.rank format ";
        query.append(format_spec);

        if (use_mpi) {
            config()["CALI_SERVICES_ENABLE"].append(",mpi,mpireport");
            config()["CALI_AGGREGATE_KEY"]               = "mpi.rank";
            config()["CALI_MPIREPORT_FILENAME"]          = output;
            config()["CALI_MPIREPORT_WRITE_ON_FINALIZE"] = "false";
            config()["CALI_MPIREPORT_CONFIG"]            = opts.build_query("local", query);
        } else {
            config()["CALI_SERVICES_ENABLE"].append(",report");
            config()["CALI_REPORT_FILENAME"] = output;
            config()["CALI_REPORT_CONFIG"]   = opts.build_query("local", query);
        }

        opts.update_channel_config(config());
        opts.update_channel_metadata(metadata());
    }
};

std::string check_args(const cali::ConfigManager::Options& opts)
{
    // Check if output.format is valid

    std::string           format          = opts.get("output.format", "json-split");
    std::set<std::string> allowed_formats = { "cali", "json", "json-split", "hatchet" };

    if (allowed_formats.find(format) == allowed_formats.end())
        return std::string("hatchet-region-profile: Invalid output format \"") + format + "\"";

    return "";
}

cali::ChannelController* make_controller(
    const char*                         name,
    const config_map_t&                 initial_cfg,
    const cali::ConfigManager::Options& opts
)
{
    std::string format = opts.get("output.format", "cali");

    if (format == "hatchet")
        format = "cali";

    if (!(format == "json-split" || format == "json" || format == "cali")) {
        format = "json-split";
        Log(0).stream() << "hatchet-region-profile: Unknown output format \"" << format << "\". Using json-split."
                        << std::endl;
    }

    return new HatchetRegionProfileController(name, initial_cfg, opts, format);
}

const char* controller_spec = R"json(
{
 "name"        : "hatchet-region-profile",
 "description" : "Record a region time profile for processing with hatchet",
 "categories"  : [ "adiak", "metadata", "metric", "output", "region", "event" ],
 "services"    : [ "aggregate", "event", "timer" ],
 "config"      :
 { "CALI_CHANNEL_FLUSH_ON_EXIT"      : "false",
   "CALI_EVENT_ENABLE_SNAPSHOT_INFO" : "false",
   "CALI_TIMER_UNIT"                 : "sec"
 },
 "defaults"    : { "node.order": "true" },
 "options":
 [
  {
   "name": "output.format",
   "type": "string",
   "description": "Output format ('hatchet', 'cali', 'json')"
  },{
   "name": "use.mpi",
   "type": "bool",
   "description": "Merge results into a single output stream in MPI programs"
  },{
   "name": "time.inclusive",
   "type": "bool",
   "category": "metric",
   "description": "Add inclusive time metric",
   "query":
   [
    {
     "level"  : "local",
     "select" : [ "inclusive_scale(sum#time.duration.ns,1e-9) as \"time (inc)\" unit sec" ]
    }
   ]
  }
 ]
}
)json";

} // namespace

namespace cali
{

ConfigManager::ConfigInfo hatchet_region_profile_controller_info { ::controller_spec, ::make_controller, ::check_args };

}
