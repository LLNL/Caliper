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

// List of services, defined in services.inc.cpp
#include "services.inc.cpp"

namespace
{

class ServicesManager {
    std::vector<CaliperService> m_services;

public:

    std::vector<std::string> get_available_services() const {
        std::vector<std::string> ret;
        ret.reserve(m_services.size());

        for (const CaliperService& s : m_services)
            ret.push_back(s.name);

        return ret;
    }

    bool register_service(const char* name, Caliper* c, Channel* channel) const {
        for (const CaliperService& s : m_services)
            if (strcmp(s.name, name) == 0 && s.register_fn) {
                (*s.register_fn)(c, channel);
                return true;
            }

        return false;
    }

    void add_services(const CaliperService services_list[]) {
        for (const CaliperService* s = services_list; s && s->name && s->register_fn; ++s)
            if (std::find_if(m_services.begin(), m_services.end(),
                    [s](const CaliperService& ls){
                        // string pointer compare is fine here
                        return s->name == ls.name && s->register_fn == ls.register_fn;
                    }) == m_services.end())
                        m_services.push_back(*s);
    }

    static ServicesManager* instance() {
        static std::unique_ptr<ServicesManager> inst { new ServicesManager };
        return inst.get();
    }
};

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

bool register_service(Caliper* c, Channel* channel, const char* name)
{
    return ServicesManager::instance()->register_service(name, c, channel);
}

void register_configured_services(Caliper* c, Channel* channel)
{
    std::vector<std::string> services =
        channel->config().init("services", s_configdata).get("enable").to_stringlist(",:");

    ServicesManager* sM = ServicesManager::instance();

    for (const std::string& s : services) {
        if (!sM->register_service(s.c_str(), c, channel))
            Log(0).stream() << "Service \"" << s << "\" not found!" << std::endl;
    }
}

void add_service_specs(const CaliperService* services)
{
    ServicesManager::instance()->add_services(services);
}

std::vector<std::string> get_available_services()
{
    return ServicesManager::instance()->get_available_services();
}

} // namespace services

} // namespace cali
