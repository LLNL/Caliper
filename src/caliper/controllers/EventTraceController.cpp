// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

using namespace cali;

namespace
{

class EventTraceController : public cali::ChannelController
{
public:

    EventTraceController(const char* name, const config_map_t& initial_cfg, const ConfigManager::Options& opts)
        : ChannelController(name, 0, initial_cfg)
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

static cali::ChannelController*
make_event_trace_controller(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts)
{
    return new EventTraceController(name, initial_cfg, opts);
}

const char* event_trace_spec =
    "{"
    " \"name\"        : \"event-trace\","
    " \"description\" : \"Record a trace of region enter/exit events in .cali format\","
    " \"services\"    : [ \"event\", \"recorder\", \"timestamp\", \"trace\" ],"
    " \"categories\"  : [ \"output\" ],"
    " \"config\"      : "
    "   { \"CALI_CHANNEL_FLUSH_ON_EXIT\"   : \"false\","
    "     \"CALI_TIMER_SNAPSHOT_DURATION\" : \"true\","
    "     \"CALI_TIMER_UNIT\"              : \"sec\""
    "   },"
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
    "    \"extra_config_flags\": { \"CALI_MPI_BLACKLIST\": \"MPI_Wtime,MPI_Wtick,MPI_Comm_size,MPI_Comm_rank\" }"
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
