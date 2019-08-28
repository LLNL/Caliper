#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

#include "caliper/common/StringConverter.h"

using namespace cali;

namespace
{

enum TraceOpts {
    Timestamps = 1,
    TraceMpi   = 2,
    TraceCuda  = 4,
    TraceIo    = 8
};

class EventTraceController : public cali::ChannelController
{
public:

    EventTraceController(const std::string& output, int opts)
        : ChannelController("event-trace", 0, {
                { "CALI_CHANNEL_FLUSH_ON_EXIT", "false" },
                { "CALI_SERVICES_ENABLE", "event,recorder,trace,timestamp" },
                { "CALI_TIMER_UNIT", "sec" },
                { "CALI_TIMER_SNAPSHOT_DURATION", "true" }
            })
        {
            std::string tmp = output;

            if (!tmp.empty()) {
                if (tmp != "stderr" && tmp != "stdout") {
                    auto pos = tmp.find_last_of('.');

                    if (pos == std::string::npos || tmp.substr(pos) != ".cali")
                        tmp.append(".cali");
                }

                config()["CALI_RECORDER_FILENAME"] = tmp;
            }

            if (opts & Timestamps) {
                config()["CALI_TIMER_OFFSET"   ] = "true";
            }
            if (opts & TraceMpi) {
                config()["CALI_SERVICES_ENABLE"].append(",mpi");
                config()["CALI_MPI_BLACKLIST"  ] =
                    "MPI_Comm_size,MPI_Comm_rank,MPI_Wtime";
            }
            if (opts & TraceCuda) {
                config()["CALI_SERVICES_ENABLE"].append(",cupti");
            }
            if (opts & TraceIo) {
                config()["CALI_SERVICES_ENABLE"].append(",io");
            }
        }
};

const char* event_trace_args[] = {
    "output", "event.timestamps", "trace.io", "trace.mpi", "trace.cuda", nullptr
};

static cali::ChannelController*
make_event_trace_controller(const cali::ConfigManager::argmap_t& args)
{
    std::string output;
    auto it = args.find("output");

    if (it != args.end())
        output = it->second;

    int traceopts = 0;

    const struct {
        const char* optname; int opt;
    } trace_info[] = {
        { "event.timestamps", Timestamps },
        { "trace.mpi",        TraceMpi   },
        { "trace.cuda",       TraceCuda  },
        { "trace.io",         TraceIo    }
    };

    for (auto &info : trace_info) {
        auto it = args.find(info.optname);
        if (it != args.end() && StringConverter(it->second).to_bool())
            traceopts |= info.opt;
    }

    return new EventTraceController(output, traceopts);
}

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo event_trace_controller_info
{
    "event-trace", "event-trace(output=<filename>): Record event trace", ::event_trace_args, ::make_event_trace_controller
};

}
