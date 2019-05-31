#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

using namespace cali;

namespace
{

class RuntimeReportController : public cali::ChannelController
{
public:
    
    RuntimeReportController(bool use_mpi, const char* output)
        : ChannelController("runtime-report", 0, {
                { "CALI_CHANNEL_FLUSH_ON_EXIT", "false" }
            })
        {
            if (use_mpi) {
                config()["CALI_CONFIG_PROFILE"    ] = "mpi-runtime-report";
                config()["CALI_MPIREPORT_FILENAME"] = output;
                config()["CALI_MPIREPORT_WRITE_ON_FINALIZE"] = "false"; 
            } else {
                config()["CALI_CONFIG_PROFILE"    ] = "runtime-report";
                config()["CALI_REPORT_FILENAME"   ] = output; 
            }            
        }
};

const char* runtime_report_args[] = { "output", nullptr };

static cali::ChannelController*
make_runtime_report_controller(const cali::ConfigManager::argmap_t& args, bool use_mpi)
{
    auto it = args.find("output");
    std::string output =  (it == args.end() ? "" : "stderr");

    return new RuntimeReportController(use_mpi, output.c_str());
}

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo runtime_report_controller_info
{
    "runtime-report", ::runtime_report_args, ::make_runtime_report_controller
};

}
