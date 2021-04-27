// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

#include "caliper/common/Log.h"
#include "caliper/common/StringConverter.h"

#include "../../services/Services.h"

#include <algorithm>
#include <set>
#include <tuple>

using namespace cali;

namespace
{

class CudaActivityProfileController : public cali::ChannelController
{
public:

    CudaActivityProfileController(const char* name, const config_map_t& initial_cfg, const ConfigManager::Options& opts, const std::string& format)
        : ChannelController(name, 0, initial_cfg)
        {
            std::string output(opts.get("output", "cuda_profile").to_string());

            if (output != "stdout" && output != "stderr") {
                auto pos = output.find_last_of('.');
                std::string ext = (format == "cali" ? ".cali" : ".json");

                if (pos == std::string::npos || output.substr(pos) != ext)
                    output.append(ext);
            }

            auto avail_services = services::get_available_services();
            bool have_mpi =
                std::find(avail_services.begin(), avail_services.end(), "mpireport")    != avail_services.end();
            bool have_adiak =
                std::find(avail_services.begin(), avail_services.end(), "adiak_import") != avail_services.end();

            bool use_mpi = have_mpi;

            if (opts.is_set("use.mpi"))
                use_mpi = have_mpi && opts.is_enabled("use.mpi");

            if (have_adiak) {
                config()["CALI_SERVICES_ENABLE"].append(",adiak_import");
                config()["CALI_ADIAK_IMPORT_CATEGORIES"] =
                    opts.get("adiak.import_categories", "2,3").to_string();
            }

            if (use_mpi) {
                config()["CALI_SERVICES_ENABLE"   ].append(",mpi,mpireport");
                config()["CALI_AGGREGATE_KEY"     ] = "*,mpi.rank";
                config()["CALI_MPIREPORT_FILENAME"] = output;
                config()["CALI_MPIREPORT_WRITE_ON_FINALIZE"] = "false";
                config()["CALI_MPIREPORT_CONFIG"  ] =
                    opts.build_query("local", {
                            { "select",
                              "*,scale(cupti.activity.duration,1e-9) as \"time (gpu)\" unit sec"
                              " ,scale(sum#cupti.host.duration,1e-9) as \"time\" unit sec"
                            },
                            { "group by", "prop:nested,cupti.kernel.name,cupti.activity.kind,mpi.rank" },
                            { "format",   format }
                        });
            } else {
                config()["CALI_SERVICES_ENABLE"   ].append(",report");
                config()["CALI_REPORT_FILENAME"   ] = output;
                config()["CALI_REPORT_CONFIG"     ] =
                    opts.build_query("local", {
                            { "select",
                              "*,scale(cupti.activity.duration,1e-9) as \"time (gpu)\" unit sec"
                              " ,scale(sum#cupti.host.duration,1e-9) as \"time\" unit sec" },
                            { "group by", "prop:nested,cupti.kernel.name,cupti.activity.kind" },
                            { "format",   format }
                        });
            }

            opts.update_channel_config(config());
        }
};

std::string
check_args(const cali::ConfigManager::Options& opts) {
    // Check if output.format is valid

    std::string format = opts.get("output.format", "json-split").to_string();
    std::set<std::string> allowed_formats = { "cali", "json", "json-split", "hatchet" };

    if (allowed_formats.find(format) == allowed_formats.end())
        return std::string("cuda-activity-profile: Invalid output format \"") + format + "\"";

    return "";
}

cali::ChannelController*
make_controller(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts)
{
    std::string format = opts.get("output.format", "json-split").to_string();

    if (format == "hatchet")
        format = "json-split";

    if (!(format == "json-split" || format == "json" || format == "cali")) {
        format = "json-split";
        Log(0).stream() << "hatchet-region-profile: Unknown output format \"" << format
                        << "\". Using json-split."
                        << std::endl;
    }

    return new CudaActivityProfileController(name, initial_cfg, opts, format);
}

const char* controller_spec =
    "{"
    " \"name\"        : \"cuda-activity-profile\","
    " \"description\" : \"Record CUDA activities and a write profile\","
    " \"categories\"  : [ \"adiak\", \"metric\", \"cuptitrace.metric\", \"output\", \"region\" ],"
    " \"services\"    : [ \"aggregate\", \"cupti\", \"cuptitrace\", \"event\" ],"
    " \"config\"      : "
    "   { \"CALI_CHANNEL_FLUSH_ON_EXIT\"        : \"false\","
    "     \"CALI_EVENT_ENABLE_SNAPSHOT_INFO\"   : \"false\","
    "     \"CALI_CUPTITRACE_SNAPSHOT_DURATION\" : \"true\""
    "   },"
    " \"options\": "
    " ["
    "  { "
    "    \"name\": \"output.format\","
    "    \"type\": \"string\","
    "    \"description\": \"Output format ('hatchet', 'cali', 'json')\""
    "  },"
    "  { "
    "    \"name\": \"use.mpi\","
    "    \"type\": \"bool\","
    "    \"description\": \"Merge results into a single output stream in MPI programs\""
    "  }"
    " ]"
    "}";

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo cuda_activity_profile_controller_info
{
    ::controller_spec, ::make_controller, ::check_args
};

}
