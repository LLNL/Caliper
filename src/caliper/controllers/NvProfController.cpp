// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

#include <algorithm>

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
    "{ \"name\": \"nvprof\", \"services\": [ \"nvprof\" ] }", ::make_nvprof_controller, nullptr
};

}
