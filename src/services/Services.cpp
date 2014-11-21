/// @file Services.cpp
/// Services class implementation

#include "Services.h"

#include "MetadataWriter.h"

#include <functional>
#include <map>
#include <string>

using namespace cali;
using namespace std;

// Include list of services:
//   const Services::MetadataWriterService* metadata_writer_services
#include "services.inc.cpp"

namespace cali
{

struct Services::ServicesImpl
{
    // --- data

    static std::unique_ptr<ServicesImpl> s_instance;

    map< string, function<unique_ptr<MetadataWriter>()> > m_metadata_writers;


    // --- interface

    void register_services(Caliper* c) {
        // register caliper services

        for (const CaliperService* s = caliper_services; s->name && s->register_fn; ++s)
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

    static ServicesImpl* instance() {
        if (!s_instance)
            s_instance.reset(new ServicesImpl);

        return s_instance.get();
    }
};

unique_ptr<Services::ServicesImpl> Services::ServicesImpl::s_instance { nullptr };

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
