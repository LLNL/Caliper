// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
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

class CallpathSampleReportController : public cali::ChannelController
{
public:

    CallpathSampleReportController(bool use_mpi, const char* name, const config_map_t& initial_cfg, const ConfigManager::Options& opts)
        : ChannelController(name, 0, initial_cfg)
        {
            double freq = std::max(1.0, std::stod(opts.get("sample.frequency", "200").to_string()));

            config()["CALI_SAMPLER_FREQUENCY"] = std::to_string(freq);

            // Config for first aggregation step in MPI mode (process-local aggregation)
            std::string local_select = std::string("count() as \"Samples\",scale_count(")
                + std::to_string(1.0/freq)
                + ") as \"Time (sec)\" unit sec";

            std::string format = std::string("tree(source.function#callpath.address,")
                + opts.get("max_column_width", "48").to_string()
                + ")";

            // Config for second aggregation step in MPI mode (cross-process aggregation)
            std::string cross_select =
                  " min(scount) as \"Min time/rank\" unit sec"
                  ",max(scount) as \"Max time/rank\" unit sec"
                  ",avg(scount) as \"Avg time/rank\" unit sec"
                  ",sum(scount) as \"Total time\" unit sec"
                  ",percent_total(scount) as \"Time %\"";

            auto avail_services = services::get_available_services();
            bool have_pthread =
                std::find(avail_services.begin(), avail_services.end(), "pthread")      != avail_services.end();

            if (have_pthread)
                config()["CALI_SERVICES_ENABLE"].append(",pthread");

            if (use_mpi) {
                config()["CALI_SERVICES_ENABLE"   ].append(",mpi,mpireport");
                config()["CALI_MPIREPORT_FILENAME"] = opts.get("output", "stderr").to_string();
                config()["CALI_MPIREPORT_WRITE_ON_FINALIZE"] = "false";
                config()["CALI_MPIREPORT_LOCAL_CONFIG"] =
                    opts.build_query("local", {
                            { "select", local_select },
                            { "group by", "source.function#callpath.address" }
                        });
                config()["CALI_MPIREPORT_CONFIG"  ] =
                    opts.build_query("cross", {
                            { "select",   cross_select },
                            { "group by", "source.function#callpath.address" },
                            { "format",   format }
                        });
            } else {
                config()["CALI_SERVICES_ENABLE"   ].append(",report");
                config()["CALI_REPORT_FILENAME"   ] = opts.get("output", "stderr").to_string();
                config()["CALI_REPORT_CONFIG"     ] =
                    opts.build_query("local", {
                            { "select",   local_select },
                            { "group by", "source.function#callpath.address" },
                            { "format",   format }
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
        Log(0).stream() << "runtime-report: cannot enable mpi support: mpireport service is not available."
                        << std::endl;
    }

    return use_mpi;
}

cali::ChannelController*
make_callpath_sample_report_controller(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts)
{
    return new CallpathSampleReportController(use_mpi(opts), name, initial_cfg, opts);
}

const char* callpath_sample_report_spec =
    "{"
    " \"name\"        : \"callpath-sample-report\","
    " \"description\" : \"Print a call-path sampling profile for the program\","
    " \"categories\"  : [ \"metric\", \"output\" ],"
    " \"services\"    : [ \"callpath\", \"sampler\", \"symbollookup\", \"trace\" ],"
    " \"config\"      : "
    "   { \"CALI_CHANNEL_FLUSH_ON_EXIT\"      : \"false\""
    "   },"
    " \"options\": "
    " ["
    "  { "
    "    \"name\": \"sample.frequency\","
    "    \"type\": \"int\","
    "    \"description\": \"Sampling frequency in Hz. Default: 200\""
    "  },"
    "  {"
    "   \"name\": \"aggregate_across_ranks\","
    "   \"type\": \"bool\","
    "   \"description\": \"Aggregate results across MPI ranks\""
    "  },"
    "  {"
    "   \"name\": \"max_column_width\","
    "   \"type\": \"int\","
    "   \"description\": \"Maximum column width in the tree display\""
    "  }"
    " ]"
    "}";

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo callpath_sample_report_controller_info
{
    ::callpath_sample_report_spec, ::make_callpath_sample_report_controller, nullptr
};

}
