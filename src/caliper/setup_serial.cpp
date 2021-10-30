// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "caliper/ConfigManager.h"

#include "../services/Services.h"

namespace cali
{

extern ConfigManager::ConfigInfo spot_controller_info_serial;

void add_submodule_controllers_and_services()
{
    services::add_default_service_specs();

    const ConfigManager::ConfigInfo* controllers[] = {
        &spot_controller_info_serial,
        nullptr
    };

    add_global_config_specs(controllers);
}

void init_submodules()
{
    add_submodule_controllers_and_services();
}

} // namespace cali
