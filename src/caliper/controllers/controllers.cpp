// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "caliper/ConfigManager.h"

namespace cali
{

extern ConfigManager::ConfigInfo event_trace_controller_info;
extern ConfigManager::ConfigInfo nvprof_controller_info;
extern ConfigManager::ConfigInfo runtime_report_controller_info;
extern ConfigManager::ConfigInfo hatchet_region_profile_controller_info;
#ifdef CALIPER_HAVE_SAMPLER
extern ConfigManager::ConfigInfo hatchet_sample_profile_controller_info;
#endif

ConfigManager::ConfigInfo* builtin_controllers_table[] = {
    &event_trace_controller_info,
    &nvprof_controller_info,
    &runtime_report_controller_info,
    &hatchet_region_profile_controller_info,
#ifdef CALIPER_HAVE_SAMPLER
    &hatchet_sample_profile_controller_info,
#endif
    nullptr
};

}
