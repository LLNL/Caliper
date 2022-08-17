// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "Services.h"

#include "caliper/Caliper.h"
#include "caliper/CaliperService.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/StringConverter.h"

#include <cassert>
#include <cctype>
#include <string>
#include <sstream>

using namespace cali;

// List of services, defined in services.inc.cpp
#include "services.inc.cpp"

namespace
{

std::string get_name_from_spec(const char* name_or_spec)
{
    // try to parse the given string as spec, otherwise return it as-is
    bool ok = false;
    auto dict = StringConverter(name_or_spec).rec_dict(&ok);

    if (!ok)
        return std::string { name_or_spec };

    auto it = dict.find("name");
    if (it != dict.end())
        return it->second.to_string();

    return std::string { name_or_spec };
}

class ServicesManager {
    std::map<std::string, CaliperService> m_services;

public:

    std::vector<std::string> get_available_services() const {
        std::vector<std::string> ret;
        ret.reserve(m_services.size());

        for (const auto& p : m_services)
            ret.push_back(p.first);

        return ret;
    }

    bool register_service(const char* name, Caliper* c, Channel* channel) const {
        for (const auto &p : m_services)
            if (p.first == name && p.second.register_fn) {
                (*p.second.register_fn)(c, channel);
                return true;
            }

        return false;
    }

    void add_services(const CaliperService list[]) {
        for (const CaliperService* s = list; s && s->name_or_spec && s->register_fn; ++s)
            m_services[get_name_from_spec(s->name_or_spec)] = *s;
    }

    std::ostream& print_service_description(std::ostream& os, const std::string& name) {
        auto service_itr = m_services.find(name);
        if (service_itr == m_services.end())
            return os;

        auto dict = StringConverter(service_itr->second.name_or_spec).rec_dict();
        auto spec_itr = dict.find("description");
        if (spec_itr != dict.end())
            os << ' ' << spec_itr->second.to_string() << '\n';
        else
            os << " (no description)\n";

        spec_itr = dict.find("config");
        if (spec_itr == dict.end())
            return os;

        // print description of config variables
        auto list = spec_itr->second.rec_list();
        for (const auto& s : list) {
            auto cfg_dict = s.rec_dict();
            auto it = cfg_dict.find("name");
            if (it == cfg_dict.end())
                continue;

            std::string key = it->second.to_string();
            if (key.empty())
                continue;
            std::string str = std::string("CALI_").append(name).append("_").append(key);
            transform(str.begin(), str.end(), str.begin(), ::toupper);

            os << "  " << str;

            std::string val;
            it = cfg_dict.find("value");
            if (it != cfg_dict.end())
                val = it->second.to_string();
            if (!val.empty())
                os << '=' << val;

            it = cfg_dict.find("type");
            if (it != cfg_dict.end())
                os << " (" << it->second.to_string() << ")\n";

            it = cfg_dict.find("description");
            if (it != cfg_dict.end())
                os << "   " << it->second.to_string() << '\n';
            else
                os << "   (no description)\n";
        }

        return os;
    }

    static ServicesManager* instance() {
        static std::unique_ptr<ServicesManager> inst { new ServicesManager };
        return inst.get();
    }
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
    const RuntimeConfig::config_entry_list_t configdata {
        { "enable", "" }
    };

    std::vector<std::string> services =
        channel->config().init("services", configdata).get("enable").to_stringlist(",:");

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

std::ostream& print_service_description(std::ostream& os, const char* name)
{
    return ServicesManager::instance()->print_service_description(os, name);
}

ConfigSet init_config_from_spec(RuntimeConfig config, const char* spec)
{
    RuntimeConfig::config_entry_list_t list;

    auto dict = StringConverter(spec).rec_dict();
    auto spec_itr = dict.find("config");
    if (spec_itr != dict.end()) {
        for (const auto &e : spec_itr->second.rec_list()) {
            auto cfg_dict = e.rec_dict();

            std::string key, val;
            auto itr = cfg_dict.find("name");
            assert(itr != cfg_dict.end() && "config entry name missing");
            key = itr->second.to_string();
            itr = cfg_dict.find("value");
            if (itr != cfg_dict.end())
                val = itr->second.to_string();

            list.emplace_back(std::make_pair(key, val));
        }
    }

    spec_itr = dict.find("name");
    assert(spec_itr != dict.end() && "service name missing");
    std::string name = spec_itr->second.to_string();

    return config.init(name.c_str(), list);
}

} // namespace services

} // namespace cali
