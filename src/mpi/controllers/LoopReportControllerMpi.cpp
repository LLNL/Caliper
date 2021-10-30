#include "../../caliper/controllers/LoopReportController.h"

#include "../OutputCommMpi.h"

#include "caliper/common/OutputStream.h"

using namespace cali;

namespace
{

class LoopReportControllerMpi : public LoopReportController
{
public:

    void flush() override {
        OutputCommMpi comm;
        OutputStream stream;
        collective_flush(comm, stream);
    }

    LoopReportControllerMpi(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts)
        : LoopReportController(name, initial_cfg, opts)
    { }
};

cali::ChannelController*
make_loopreport_controller_mpi(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts) {
    return new LoopReportControllerMpi(name, initial_cfg, opts);
}

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo loopreport_controller_info_mpi
{
    ::LoopReportController::spec, ::make_loopreport_controller_mpi, nullptr
};

} // namespace cali
