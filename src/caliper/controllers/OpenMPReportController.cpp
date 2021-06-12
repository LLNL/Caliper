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

class OpenMPReportController : public cali::ChannelController
{
public:

    OpenMPReportController(bool use_mpi, const char* name, const config_map_t& initial_cfg, const ConfigManager::Options& opts)
        : ChannelController(name, 0, initial_cfg)
        {
            const char* let =
                " n.threads=first(omp.num.threads)"
                ",t.initial=first(sum#time.duration) if omp.thread.type=initial";

            // Config for first aggregation step in MPI mode (process-local aggregation)
            std::string local_select =
                " max(n.threads)"
                ",inclusive_sum(t.initial)"
                ",inclusive_sum(sum#time.duration)";
            // Config for serial-mode aggregation
            std::string serial_select =
                " max(n.threads) as \"#Threads\""
                ",inclusive_sum(t.initial) as \"Time (thread)\""
                ",inclusive_sum(sum#time.duration) as \"Time (total)\"";

            // Config for second aggregation step in MPI mode (cross-process aggregation)
            std::string cross_select =
                " max(max#n.threads) as \"#Threads\""
                ",avg(inclusive#t.initial) as \"Time (thread) (avg)\""
                ",sum(inclusive#sum#time.duration) as \"Time (total)\"";

            std::string groupby;
            std::string format("table");

            if (opts.is_enabled("show_threads")) {
                groupby.append(groupby.empty() ? "omp.thread.id"   : ",omp.thread.id");

                serial_select.append(",omp.thread.id as Thread");
                cross_select.append(",omp.thread.id as Thread");
            }

            if (opts.is_enabled("show_thread_type")) {
                groupby.append(groupby.empty() ? "omp.thread.type" : ",omp.thread.type");

                serial_select.append(",omp.thread.type as Type");
                cross_select.append(",omp.thread.type as Type");
            }

            if (opts.is_enabled("show_regions")) {
                groupby.append(groupby.empty() ? "prop:nested" : ",prop:nested");
                format = "tree";
            }

            if (groupby.empty())
                groupby = "none";

            if (use_mpi) {
                config()["CALI_SERVICES_ENABLE"   ].append(",mpi,mpireport");
                config()["CALI_MPIREPORT_FILENAME"] = opts.get("output", "stderr").to_string();
                config()["CALI_MPIREPORT_WRITE_ON_FINALIZE"] = "false";
                config()["CALI_MPIREPORT_LOCAL_CONFIG"] =
                    opts.build_query("local", {
                            { "let",      let     },
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
                            { "let",      let     },
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
        Log(0).stream() << "openmp-report: cannot enable mpi support: mpireport service is not available."
                        << std::endl;
    }

    return use_mpi;
}

cali::ChannelController*
make_controller(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts)
{
    return new OpenMPReportController(use_mpi(opts), name, initial_cfg, opts);
}

const char* controller_spec =
    "{"
    " \"name\"        : \"openmp-report\","
    " \"description\" : \"Record and print OpenMP performance metrics (loops, barriers, etc.)\","
    " \"categories\"  : [ \"output\", \"region\", \"metric\" ],"
    " \"services\"    : [ \"aggregate\", \"ompt\", \"event\" ],"
    " \"config\"      : "
    "   { \"CALI_CHANNEL_FLUSH_ON_EXIT\"        : \"false\","
    "     \"CALI_EVENT_ENABLE_SNAPSHOT_INFO\"   : \"false\""
    "   },"
    " \"defaults\"    : "
    "   { \"openmp.times\"      : \"true\","
    "     \"openmp.efficiency\" : \"true\","
    "     \"show_regions\"      : \"true\""
    "   },"
    " \"options\": "
    " ["
    "  {"
    "   \"name\": \"aggregate_across_ranks\","
    "   \"type\": \"bool\","
    "   \"description\": \"Aggregate results across MPI ranks\""
    "  },"
    "  {"
    "   \"name\": \"show_threads\","
    "   \"type\": \"bool\","
    "   \"description\": \"Show thread IDs\""
    "  },"
    "  {"
    "   \"name\": \"show_thread_type\","
    "   \"type\": \"bool\","
    "   \"description\": \"Show thread type (initial vs. worker)\""
    "  },"
    "  {"
    "   \"name\": \"show_regions\","
    "   \"type\": \"bool\","
    "   \"description\": \"Show Caliper region tree\""
    "  }"
    " ]"
    "}";

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo openmp_report_controller_info
{
    ::controller_spec, ::make_controller, nullptr
};

}
