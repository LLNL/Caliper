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

class RuntimeReportController : public cali::ChannelController
{
public:

    RuntimeReportController(bool use_mpi, const char* name, const config_map_t& initial_cfg, const ConfigManager::Options& opts)
        : ChannelController(name, 0, initial_cfg)
        {
            // Config for first aggregation step in MPI mode (process-local aggregation)
            std::string local_select =
                " sum(sum#time.duration)";
            // Config for serial-mode aggregation
            std::string serial_select =
                " sum(sum#time.duration) as \"Time (E)\""
                ",inclusive_sum(sum#time.duration) as \"Time (I)\""
                ",percent_total(sum#time.duration) as \"Time % (E)\""
                ",inclusive_percent_total(sum#time.duration) as \"Time % (I)\"";

            std::string tmetric = "sum#sum#time.duration";
            std::string pmetric = "percent_total(sum#sum#time.duration)";

            if (opts.is_enabled("calc.inclusive")) {
                local_select += ",inclusive_sum(sum#time.duration)";
                tmetric = "inclusive#sum#time.duration";
                pmetric = "inclusive_percent_total(sum#sum#time.duration)";
            }

            std::string format = "tree";

            if (opts.is_set("max_column_width")) {
                format = "(prop:nested,";
                format.append(opts.get("max_column_width").to_string());
                format.append(")");
            }

            // Config for second aggregation step in MPI mode (cross-process aggregation)
            std::string cross_select =
                  std::string(" min(") + tmetric + ") as \"Min time/rank\""
                + std::string(",max(") + tmetric + ") as \"Max time/rank\""
                + std::string(",avg(") + tmetric + ") as \"Avg time/rank\""
                + std::string(","    ) + pmetric + "  as \"Time %\"";

            if (use_mpi) {
                config()["CALI_SERVICES_ENABLE"   ].append(",mpi,mpireport");
                config()["CALI_MPIREPORT_FILENAME"] = opts.get("output", "stderr").to_string();
                config()["CALI_MPIREPORT_WRITE_ON_FINALIZE"] = "false";
                config()["CALI_MPIREPORT_LOCAL_CONFIG"] =
                    opts.build_query("local", {
                            { "select",   local_select  },
                            { "group by", "prop:nested" }
                        });
                config()["CALI_MPIREPORT_CONFIG"  ] =
                    opts.build_query("cross", {
                            { "select",   cross_select  },
                            { "group by", "prop:nested" },
                            { "format",   format        }
                        });
            } else {
                config()["CALI_SERVICES_ENABLE"   ].append(",report");
                config()["CALI_REPORT_FILENAME"   ] = opts.get("output", "stderr").to_string();
                config()["CALI_REPORT_CONFIG"     ] =
                    opts.build_query("local", {
                            { "select",   serial_select },
                            { "group by", "prop:nested" },
                            { "format",   format        }
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
make_runtime_report_controller(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts)
{
    return new RuntimeReportController(use_mpi(opts), name, initial_cfg, opts);
}

const char* runtime_report_spec =
    "{"
    " \"name\"        : \"runtime-report\","
    " \"description\" : \"Print a time profile for annotated regions\","
    " \"categories\"  : [ \"metric\", \"output\", \"region\" ],"
    " \"services\"    : [ \"aggregate\", \"event\", \"timestamp\" ],"
    " \"config\"      : "
    "   { \"CALI_CHANNEL_FLUSH_ON_EXIT\"      : \"false\","
    "     \"CALI_EVENT_ENABLE_SNAPSHOT_INFO\" : \"false\","
    "     \"CALI_TIMER_SNAPSHOT_DURATION\"    : \"true\","
    "     \"CALI_TIMER_INCLUSIVE_DURATION\"   : \"false\","
    "     \"CALI_TIMER_UNIT\"                 : \"sec\""
    "   },"
    " \"options\": "
    " ["
    "  {"
    "   \"name\": \"calc.inclusive\","
    "   \"type\": \"bool\","
    "   \"description\": \"Report inclusive instead of exclusive times\""
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

ConfigManager::ConfigInfo runtime_report_controller_info
{
    ::runtime_report_spec, ::make_runtime_report_controller, nullptr
};

}
