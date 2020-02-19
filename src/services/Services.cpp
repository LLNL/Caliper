// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "Services.h"

#include "caliper/Caliper.h"
#include "caliper/CaliperService.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>

using namespace cali;
using namespace std;

// List of services, defined in services.inc.cpp
#include "services.inc.cpp"

namespace
{

struct ServicesList {
    const CaliperService* services;
    ServicesList* next;
};

ServicesList*          s_services_list  { nullptr };
const ConfigSet::Entry s_configdata[] = {
    // key, type, value, short description, long description
    { "enable", CALI_TYPE_STRING, "",
      "List of service modules to enable",
      "A list of comma-separated names of the service modules to enable"      
    },
    ConfigSet::Terminator
};

} // namespace [anonymous]


namespace cali
{

namespace services
{

bool register_service(Caliper* c, Channel* chn, const char* name) 
{
    for (const ServicesList* lp = ::s_services_list; lp; lp = lp->next)
        for (const CaliperService* s = lp->services; s->name && s->register_fn; ++s)
            if (strcmp(s->name, name) == 0) {
                (*s->register_fn)(c, chn);
                return true;
            }

    return false;
}

void register_configured_services(Caliper* c, Channel* chn) 
{
    // list services
    
    vector<string> services =
        chn->config().init("services", s_configdata).get("enable").to_stringlist(",:");

    // register caliper services

    for (const ServicesList* lp = ::s_services_list; lp; lp = lp->next)
        for (const CaliperService* s = lp->services; s->name && s->register_fn; ++s) {
            auto it = find(services.begin(), services.end(), string(s->name));

            if (it != services.end()) {
                (*s->register_fn)(c, chn);
                services.erase(it);
            }
        }

    for ( const string& s : services )
        Log(0).stream() << "Warning: service \"" << s << "\" not found" << endl;
}

void add_service_specs(const CaliperService* services)
{
    ::ServicesList* elem = new ServicesList { services, ::s_services_list };
    ::s_services_list = elem;
}

void cleanup_service_specs()
{
    ::ServicesList* ptr = ::s_services_list;

    while (ptr) {
        ::ServicesList* tmp = ptr->next;
        delete ptr;
        ptr = tmp;
    }

    ::s_services_list = nullptr;
}

void add_default_service_specs()
{
    for (const ServicesList* lp = ::s_services_list; lp; lp = lp->next)
        if (lp->services == caliper_services)
            return;
    
    add_service_specs(caliper_services);
}

std::vector<std::string> get_available_services()
{
    std::vector<std::string> ret;
    
    for (const ServicesList* lp = ::s_services_list; lp; lp = lp->next)
        for (const CaliperService* s = lp->services; s->name && s->register_fn; ++s)
            ret.emplace_back(s->name);

    return ret;
}

} // namespace services

} // namespace cali