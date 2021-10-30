#include "SpotController.h"

#include "caliper/common/OutputStream.h"

using namespace cali;

namespace
{

class SpotControllerSerial : public SpotController
{
public:

    void flush() override {
        cali::internal::CustomOutputController::Comm comm;
        OutputStream stream;
        collective_flush(comm, stream);
    }

    SpotControllerSerial(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts)
        : SpotController(name, initial_cfg, opts)
    { }
};

cali::ChannelController*
make_spot_controller_serial(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts) {
    return new SpotControllerSerial(name, initial_cfg, opts);
}

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo spot_controller_info_serial
{
    ::SpotController::spec, ::make_spot_controller_serial, ::SpotController::check_options
};

}
