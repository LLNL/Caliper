#include "RuntimeProfileController.hpp"

using namespace cali;

RuntimeProfileController::RuntimeProfileController(bool use_mpi, const char* output)
    : ChannelController("runtime_report", 0, {
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
