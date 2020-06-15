// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "../services/Services.h"

namespace cali
{

void add_submodule_controllers_and_services()
{
    services::add_default_service_specs();
}

void init_submodules()
{
    add_submodule_controllers_and_services();
}

} // namespace cali
