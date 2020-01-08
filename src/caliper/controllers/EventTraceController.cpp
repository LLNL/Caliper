// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

#include "caliper/common/StringConverter.h"

#include "../../services/Services.h"

#include <algorithm>

using namespace cali;

namespace
{

// enum TraceOpts {
//     Timestamps = 1,
//     TraceMpi   = 2,
//     TraceCuda  = 4,
//     TraceIo    = 8
// };

class EventTraceController : public cali::ChannelController
{
public:

    EventTraceController(const ConfigManager::Options& opts)
        : ChannelController("event-trace", 0, {
                { "CALI_CHANNEL_FLUSH_ON_EXIT", "false" },
                { "CALI_SERVICES_ENABLE", "event,recorder,timestamp,trace" },
                { "CALI_TIMER_UNIT", "sec" },
                { "CALI_TIMER_SNAPSHOT_DURATION", "true" }
            })
        {
            std::string tmp = opts.get("output", "").to_string();

            if (!tmp.empty()) {
                if (tmp != "stderr" && tmp != "stdout") {
                    auto pos = tmp.find_last_of('.');

                    if (pos == std::string::npos || tmp.substr(pos) != ".cali")
                        tmp.append(".cali");
                }

                config()["CALI_RECORDER_FILENAME"] = tmp;
            }

            opts.update_channel_config(config());
        }
};

std::string
check_args(const cali::ConfigManager::Options& opts) {
    //
    //   Check if the required services for all requested profiling options
    // are there
    //

    // const struct opt_info_t {
    //     const char* option;
    //     const char* service;
    // } opt_info_list[] = {
    //     { "trace.io",     "io"    },
    //     { "trace.mpi",    "mpi"   },
    //     { "trace.cuda",   "cupti" }
    // };

    // Services::add_default_services();
    // auto svcs = Services::get_available_services();

    // for (const opt_info_t o : opt_info_list) {
    //     auto it = args.find(o.option);

    //     if (it != args.end()) {
    //         bool ok = false;

    //         if (StringConverter(it->second).to_bool(&ok) == true) 
    //             if (std::find(svcs.begin(), svcs.end(), o.service) == svcs.end())
    //                 return std::string("event-trace: ") 
    //                     + o.service
    //                     + std::string(" service required for ")
    //                     + o.option
    //                     + std::string(" option is not available");

    //         if (!ok) // parse error
    //             return std::string("event-trace: Invalid value \"") 
    //                 + it->second + "\" for " 
    //                 + it->first;
    //     }
    // }

    return "";
}

static cali::ChannelController*
make_event_trace_controller(const cali::ConfigManager::Options& opts)
{
    return new EventTraceController(opts);
}

const char* event_trace_spec = 
    "{"
    " \"name\"        : \"event-trace\","
    " \"description\" : \"Record a trace of region enter/exit events in .cali format\","
    " \"categories\"  : [ \"output\" ],"
    " \"options\": "
    " ["
    "  { \"name\"        : \"trace.io\","
    "    \"description\" : \"Trace I/O events\","
    "    \"type\"        : \"bool\","
    "    \"services\"    : [ \"io\" ] "
    "  },"
    "  { \"name\"        : \"trace.mpi\","
    "    \"description\" : \"Trace I/O events\","
    "    \"type\"        : \"bool\","
    "    \"services\"    : [ \"mpi\" ],"
    "    \"extra_config_flags\": { \"CALI_MPI_BLACKLIST\": \"MPI_Wtime,MPI_Wtick,MPI_Comm_size,MPI_comm_rank\" }"
    "  },"
    "  { \"name\"        : \"trace.cuda\","
    "    \"description\" : \"Trace CUDA API events\","
    "    \"type\"        : \"bool\","
    "    \"services\"    : [ \"cupti\" ]"
    "  },"
    "  { \"name\"        : \"trace.io\","
    "    \"description\" : \"Trace I/O events\","
    "    \"type\"        : \"bool\","
    "    \"services\"    : [ \"io\" ] "
    "  },"
    "  { \"name\"        : \"event.timestamps\","
    "    \"description\" : \"Record event timestamps\","
    "    \"type\"        : \"bool\","
    "    \"extra_config_flags\": { \"CALI_TIMER_OFFSET\": \"true\" }"
    "  }"
    " ]"
    "}";

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo event_trace_controller_info
{
    ::event_trace_spec, ::make_event_trace_controller, nullptr
};

}
