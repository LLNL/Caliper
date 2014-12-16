/// @file Services.cpp
/// Services class implementation

#include "Services.h"

#include "CaliperService.h"

#include <RuntimeConfig.h>
#include <MetadataWriter.h>

#include <util/split.hpp>

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <string>

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

    map< string, function<unique_ptr<MetadataWriter>()> > m_metadata_writers;


    // --- interface

    void register_services(Caliper* c) {
        vector<string> services;

        util::split(m_config.get("enable").to_string(), ':', back_inserter(services));

        // register caliper services

        for (const CaliperService* s = caliper_services; s->name && s->register_fn; ++s)
            if (find(services.begin(), services.end(), string(s->name)) != services.end())
                (s->register_fn)(c);

        // register metadata writers

        for (const MetadataWriterService* s = metadata_writer_services; s->name && s->create_fn; ++s) {
            if (s->register_fn)
                (s->register_fn)();

            function<unique_ptr<MetadataWriter>()> create_fn = s->create_fn;

            m_metadata_writers.insert(make_pair(string(s->name), create_fn));
        }
    }

    unique_ptr<MetadataWriter> get_metadata_writer(const char* name) {
        auto it = m_metadata_writers.find(name);

        if (it == m_metadata_writers.end())
            return { nullptr };

        return (it->second)();
    }

    void print_writers(ostream& os) {
        for ( auto const &it : m_metadata_writers )
            os << it.first;
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

unique_ptr<MetadataWriter> Services::get_metadata_writer(const char* name)
{
    return ServicesImpl::instance()->get_metadata_writer(name);
}
