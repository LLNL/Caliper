// Copyright (c) 2017, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Perform Caliper configuration sanity check

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <algorithm>
#include <iterator>

using namespace cali;

namespace
{

const char* trigger_grp[] = { "event", "sampler", "libpfm", "alloc", nullptr };
const char* buffer_grp[]  = { "aggregate", "trace", nullptr };
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

void config_sanity_check(RuntimeConfig* cfg)
{
    std::vector<std::string> services =
        cfg->get("services", "enable").to_stringlist(",:");

    if (services.empty()) {
        Log(1).stream() << "Config check: No services enabled. Caliper will not record data."
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
