/// @file Services.cpp
/// Services class implementation

#include "caliper-config.h"

#include "Services.h"

#include "CaliperService.h"

#include <Log.h>
#include <RuntimeConfig.h>
#include <util/split.hpp>

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <string>
#include <sstream>
#include <vector>

using namespace cali;
using namespace std;

// Include list of services:
//   const MetadataWriterService metadata_writer_services[]
//   const CaliperService caliper_services[]
#include "services.inc.cpp"

namespace cali
{

struct Services::ServicesImpl
{
    // --- data

    static std::unique_ptr<ServicesImpl> s_instance;
    static const ConfigSet::Entry        s_configdata[];

    ConfigSet m_config;


    // --- interface

    void register_services(Caliper* c) {
        // list services

        if (Log::verbosity() >= 2) {
            ostringstream sstr;

            for (const CaliperService* s = caliper_services; s->name && s->register_fn; ++s)
                sstr << ' ' << s->name;

            Log(2).stream() << "Available services:" << sstr.str() << std::endl;
        }

        vector<string> services;

        util::split(m_config.get("enable").to_string(), ':', back_inserter(services));

        // register caliper services

        for (const CaliperService* s = caliper_services; s->name && s->register_fn; ++s) {
            auto it = find(services.begin(), services.end(), string(s->name));

            if (it != services.end()) {
                (*s->register_fn)(c);
                services.erase(it);
            }
        }

        for ( const string& s : services )
            Log(0).stream() << "Warning: service \"" << s << "\" not found" << endl;
    }

    ServicesImpl()
        : m_config { RuntimeConfig::init("services", s_configdata) }
        { }

    static ServicesImpl* instance() {
        if (!s_instance)
            s_instance.reset(new ServicesImpl);

        return s_instance.get();
    }
};

unique_ptr<Services::ServicesImpl> Services::ServicesImpl::s_instance { nullptr };

const ConfigSet::Entry             Services::ServicesImpl::s_configdata[] = {
    // key, type, value, short description, long description
    { "enable", CALI_TYPE_STRING, "",
      "List of service modules to enable",
      "A list of colon-separated names of the service modules to enable"      
    },
    ConfigSet::Terminator
};

} // namespace cali


//
// --- Services public interface
//

void Services::register_services(Caliper* c)
{
    return ServicesImpl::instance()->register_services(c);
}
