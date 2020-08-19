// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Perform Caliper configuration sanity check

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <algorithm>
#include <iterator>

using namespace cali;

namespace
{

const char* trigger_grp[] = {
    "alloc",
    "cuptitrace",
    "event",
    "libpfm",
    "loop_monitor",
    "region_monitor",
    "sampler",
    nullptr
};
const char* buffer_grp[]  = { "aggregate", "trace", "cuptitrace", nullptr };
const char* process_grp[] = { "aggregate", "trace", "textlog", nullptr };
const char* online_grp[]  = { "textlog", nullptr };
const char* offline_grp[] = { "recorder", "report", "sos", "mpireport", nullptr };

struct service_group_t {
    const char*  group_name;
    const char** services;
} service_groups[] = {
    { "snapshot trigger", trigger_grp },
    { "snapshot buffer",  buffer_grp  },
    { "snapshot process", process_grp },
    { "online output",    online_grp  },
    { "offline output",   offline_grp }
};

enum ServiceGroupID {
    SnapshotTrigger = 0,
    SnapshotBuffer  = 1,
    SnapshotProcess = 2,
    OnlineOutput    = 3,
    OfflineOutput   = 4
};

std::string
format_group(const char* const* group, const char* sep, const char* last_sep)
{
    std::string ret;
    int count = 0;

    for (const char* const* sptr = group; sptr && *sptr; ++sptr) {
        if (count++) {
            if (*(sptr+1) == nullptr)
                ret.append(last_sep);
            else
                ret.append(sep);
        }

        ret.append("\"").append(*sptr).append("\"");
    }

    return ret;
}

void
check_service_dependency(const service_group_t dependent,
                         const service_group_t dependency,
                         const std::vector<std::string>& services)
{
    const char* const* dept = dependent.services;

    for ( ; dept && *dept; ++dept)
        if (std::find(services.begin(), services.end(), *dept) != services.end())
            break;

    if (!dept || !(*dept))
        return;

    const char* const* depcy = dependency.services;

    for ( ; depcy && *depcy; ++depcy)
        if (std::find(services.begin(), services.end(), *depcy) != services.end())
            break;

    if (depcy && *depcy)
        return;

    Log(1).stream() << "Config check: Warning: " << dependent.group_name
                    << " service \"" << *dept << "\" requires " << dependency.group_name
                    << " services, but none are active.\n     Add "
                    << format_group(dependency.services, ", ", " or ")
                    << " to CALI_SERVICES_ENABLE to generate Caliper output." << std::endl;
}

void check_services(const std::vector<std::string>& services)
{
    const struct ServiceDependency {
        ServiceGroupID dept; ServiceGroupID depcy;
    } dependencies[] {
        { SnapshotTrigger, SnapshotProcess },
        { SnapshotProcess, SnapshotTrigger },
        { SnapshotBuffer,  OfflineOutput   },
        { OnlineOutput,    SnapshotTrigger },
        { OfflineOutput,   SnapshotBuffer  }
    };

    for (const ServiceDependency &d : dependencies)
        check_service_dependency(service_groups[d.dept], service_groups[d.depcy],
                                 services);
}

} // namespace [anonymous]


namespace cali
{

void config_sanity_check(const char* channel, RuntimeConfig cfg)
{
    std::vector<std::string> services =
        cfg.get("services", "enable").to_stringlist(",:");

    if (services.empty()) {
        Log(1).stream() << channel << ": No services enabled, "
                        << channel << " channel will not record data."
                        << std::endl;
        return;
    }

    // do nothing if "debug" or "validator" are defined
    if (std::find(services.begin(), services.end(), "debug") != services.end())
        return;
    if (std::find(services.begin(), services.end(), "validator") != services.end())
        return;

    ::check_services(services);
}

}
