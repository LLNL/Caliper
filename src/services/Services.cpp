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

ServicesList* s_services_list { nullptr };

}


namespace cali
{

struct Services::ServicesImpl
{
    // --- data

    static std::unique_ptr<ServicesImpl> s_instance;
    static const ConfigSet::Entry        s_configdata[];

    // --- interface

    void register_services(Caliper* c, Channel* chn) {
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

    ServicesImpl()
        { }

    static ServicesImpl* instance() {
        if (!s_instance)
            s_instance.reset(new ServicesImpl);

        return s_instance.get();
    }
};

unique_ptr<Services::ServicesImpl> Services::ServicesImpl::s_instance      { nullptr };

const ConfigSet::Entry             Services::ServicesImpl::s_configdata[] = {
    // key, type, value, short description, long description
    { "enable", CALI_TYPE_STRING, "",
      "List of service modules to enable",
      "A list of comma-separated names of the service modules to enable"      
    },
    ConfigSet::Terminator
};

} // namespace cali


//
// --- Services public interface
//

void Services::add_services(const CaliperService* services)
{
    ::ServicesList* elem = new ServicesList { services, ::s_services_list };
    ::s_services_list = elem;
}

void Services::add_default_services()
{
    for (const ServicesList* lp = ::s_services_list; lp; lp = lp->next)
        if (lp->services == caliper_services)
            return;
    
    add_services(caliper_services);
}

void Services::register_services(Caliper* c, Channel* chn)
{
    return ServicesImpl::instance()->register_services(c, chn);
}

std::vector<std::string> Services::get_available_services()
{
    std::vector<std::string> ret;
    
    for (const ServicesList* lp = ::s_services_list; lp; lp = lp->next)
        for (const CaliperService* s = lp->services; s->name && s->register_fn; ++s)
            ret.emplace_back(s->name);

    return ret;
}
