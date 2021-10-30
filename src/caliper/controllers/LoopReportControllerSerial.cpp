#include "LoopReportController.h"

#include "caliper/common/OutputStream.h"

using namespace cali;

namespace
{

class LoopReportControllerSerial : public LoopReportController
{
public:

    void flush() override {
        cali::internal::CustomOutputController::Comm comm;
        OutputStream stream;
        collective_flush(comm, stream);
    }

    LoopReportControllerSerial(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts)
        : LoopReportController(name, initial_cfg, opts)
    { }
};

cali::ChannelController*
make_loopreport_controller_serial(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts) {
    return new LoopReportControllerSerial(name, initial_cfg, opts);
}

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo loopreport_controller_info_serial
{
    ::LoopReportController::spec, ::make_loopreport_controller_serial, nullptr
};

}
