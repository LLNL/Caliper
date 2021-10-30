#include "../../caliper/controllers/SpotController.h"

#include "../OutputCommMpi.h"

#include "caliper/common/OutputStream.h"

using namespace cali;

namespace
{

class SpotControllerMpi : public SpotController
{
public:

    void flush() override {
        OutputCommMpi comm;
        OutputStream stream;
        collective_flush(comm, stream);
    }

    SpotControllerMpi(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts)
        : SpotController(name, initial_cfg, opts)
    { }
};

cali::ChannelController*
make_spot_controller_mpi(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts) {
    return new SpotControllerMpi(name, initial_cfg, opts);
}

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo spot_controller_info_mpi
{
    ::SpotController::spec, ::make_spot_controller_mpi, ::SpotController::check_options
};

} // namespace cali
