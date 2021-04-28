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

class HatchetSampleProfileController : public cali::ChannelController
{
public:

    HatchetSampleProfileController(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts, const std::string& format)
        : ChannelController(name, 0, initial_cfg)
        {
            std::string freqstr = opts.get("sample.frequency", "200").to_string();

            config()["CALI_SAMPLER_FREQUENCY"] = freqstr;

            std::string select  = "*,count()";
            double freq = std::stod(freqstr);

            if (freq > 0) {
                select.append(",scale_count(");
                select.append(std::to_string(1.0/freq));
                select.append(") as time unit sec");
            }

            std::string output(opts.get("output", "sample_profile").to_string());

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
            bool have_pthread =
                std::find(avail_services.begin(), avail_services.end(), "pthread")      != avail_services.end();

            bool use_mpi = have_mpi;

            if (opts.is_set("use.mpi"))
                use_mpi = have_mpi && opts.is_enabled("use.mpi");

            if (have_adiak) {
                config()["CALI_SERVICES_ENABLE"].append(",adiak_import");
                config()["CALI_ADIAK_IMPORT_CATEGORIES"] =
                    opts.get("adiak.import_categories", "2,3").to_string();
            }

            if (have_pthread)
                config()["CALI_SERVICES_ENABLE"].append(",pthread");

            if (use_mpi) {
                config()["CALI_SERVICES_ENABLE"   ].append(",mpi,mpireport");
                config()["CALI_MPIREPORT_FILENAME"] = output;
                config()["CALI_MPIREPORT_WRITE_ON_FINALIZE"] = "false";
                config()["CALI_MPIREPORT_CONFIG"  ] =
                    opts.build_query("local", {
                            { "select",   select },
                            { "group by", "prop:nested,mpi.rank" },
                            { "format",   format }
                        });
            } else {
                config()["CALI_SERVICES_ENABLE"   ].append(",report");
                config()["CALI_REPORT_FILENAME"   ] = output;
                config()["CALI_REPORT_CONFIG"     ] =
                    opts.build_query("local", {
                            { "select",   select },
                            { "group by", "prop:nested" },
                            { "format",   format }
                        });
            }

            opts.update_channel_config(config());
        }
};

std::string
check_args(const cali::ConfigManager::Options& opts) {
    services::add_default_service_specs();
    auto svcs = services::get_available_services();

    //
    // Check if the sampler service is there
    //

    if (std::find(svcs.begin(), svcs.end(), "sampler") == svcs.end())
        return "hatchet-sample-profile: sampler service is not available";

    //
    // Check if output.format is valid
    //

    std::string format = opts.get("output.format", "json-split").to_string();
    std::set<std::string> allowed_formats = { "cali", "json", "json-split" };

    if (allowed_formats.find(format) == allowed_formats.end())
        return std::string("hatchet-sample-profile: Invalid output format \"") + format + "\"";

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

    return new HatchetSampleProfileController(name, initial_cfg, opts, format);
}

const char* controller_spec =
    "{"
    " \"name\"        : \"hatchet-sample-profile\","
    " \"description\" : \"Record a sampling profile for processing with hatchet\","
    " \"services\"    : [ \"sampler\", \"trace\" ],"
    " \"categories\"  : [ \"adiak\", \"output\" ],"
    " \"config\"      : { \"CALI_CHANNEL_FLUSH_ON_EXIT\": \"false\" },"
    " \"defaults\"    : { \"sample.callpath\": \"true\", \"lookup.module\": \"true\" },"
    " \"options\": "
    " ["
    "  { "
    "    \"name\": \"output.format\","
    "    \"type\": \"string\","
    "    \"description\": \"Output format ('hatchet', 'cali', 'json')\""
    "  },"
    "  { "
    "    \"name\": \"sample.frequency\","
    "    \"type\": \"int\","
    "    \"description\": \"Sampling frequency in Hz. Default: 200\""
    "  },"
    "  { "
    "    \"name\": \"sample.callpath\","
    "    \"type\": \"bool\","
    "    \"description\": \"Perform call-stack unwinding\","
    "    \"services\": [ \"callpath\", \"symbollookup\" ],"
    "    \"query\": [ { \"level\": \"local\", \"group by\": source.function#callpath.address } ]"
    "  },"
    "  { "
    "    \"name\": \"lookup.module\","
    "    \"type\": \"bool\","
    "    \"description\": \"Lookup source module (.so/.exe)\","
    "    \"services\": [ \"symbollookup\" ],"
    "    \"config\": { \"CALI_SYMBOLLOOKUP_LOOKUP_MODULE\": \"true\" },"
    "    \"query\": [ { \"level\": \"local\", \"group by\": \"module#cali.sampler.pc\" } ]"
    "  },"
    "  { "
    "    \"name\": \"lookup.sourceloc\","
    "    \"type\": \"bool\","
    "    \"description\": \"Lookup source location (file+line)\","
    "    \"services\": [ \"symbollookup\" ],"
    "    \"config\": { \"CALI_SYMBOLLOOKUP_LOOKUP_SOURCELOC\": \"true\" },"
    "    \"query\": [ { \"level\": \"local\", \"group by\": \"sourceloc#cali.sampler.pc\" } ]"
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

ConfigManager::ConfigInfo hatchet_sample_profile_controller_info
{
    ::controller_spec, ::make_controller, ::check_args
};

}
