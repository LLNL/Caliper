// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

using namespace cali;

namespace
{

cali::ChannelController*
make_nvprof_controller(const cali::ConfigManager::Options&)
{
    return new ChannelController("nvprof", 0 , {
            { "CALI_SERVICES_ENABLE",       "nvprof" },
            { "CALI_CHANNEL_FLUSH_ON_EXIT", "false"  }
        });
}

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo nvprof_controller_info
{
    "{"
    " \"name\"        : \"nvprof\","
    " \"services\"    : [ \"nvprof\" ],"
    " \"description\" : \"Forward Caliper enter/exit events to NVidia nvprof (nvtx)\""
    "}",
    ::make_nvprof_controller, nullptr
};

}
