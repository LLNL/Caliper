#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

using namespace cali;

namespace
{

class EventTraceController : public cali::ChannelController
{
public:
    
    EventTraceController(const std::string& output)
        : ChannelController("event-trace", 0, {
                { "CALI_CHANNEL_FLUSH_ON_EXIT", "false" },
                { "CALI_CONFIG_PROFILE", "serial-trace" }
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
        }
};

const char* event_trace_args[] = { "output", nullptr };

static cali::ChannelController*
make_event_trace_controller(const cali::ConfigManager::argmap_t& args, bool /*use_mpi*/ )
{
    std::string output;
    auto it = args.find("output");
    
    if (it != args.end())
        output = it->second;
        
    return new EventTraceController(output);
}

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo event_trace_controller_info
{
    "event-trace", ::event_trace_args, ::make_event_trace_controller
};

}
