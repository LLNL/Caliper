#include "caliper/ConfigManager.h"

namespace cali
{

extern ConfigManager::ConfigInfo event_trace_controller_info;
extern ConfigManager::ConfigInfo nvprof_controller_info;
extern ConfigManager::ConfigInfo runtime_report_controller_info;
extern ConfigManager::ConfigInfo hatchet_region_profile_controller_info;

ConfigManager::ConfigInfo builtin_controllers_table[] = {
    event_trace_controller_info,
    nvprof_controller_info,
    runtime_report_controller_info,
    hatchet_region_profile_controller_info,

    { nullptr, nullptr, nullptr }
};

}
