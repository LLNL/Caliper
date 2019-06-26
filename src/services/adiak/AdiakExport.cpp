// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"

#include <adiak.hpp>

#include <algorithm>
#include <map>
#include <vector>

using namespace cali;

namespace
{

std::map< cali_id_t, std::vector<Variant> >
get_caliper_globals(Caliper* c, Channel* chn)
{
    std::map< cali_id_t, std::vector<Variant> > ret;
    auto globals = c->get_globals(chn);
    
    // expand globals

    for (const Entry& e : globals) {
        if (e.is_reference()) {
            for (const Node* node = e.node(); node; node = node->parent())
                ret[node->attribute()].push_back(node->data());
        } else if (e.is_immediate()) {
            ret[e.attribute()].push_back(e.value());
        }
    }

    return ret;
}

void
export_globals_to_adiak(Caliper* c, Channel* chn)
{
    auto globals_map = get_caliper_globals(c, chn);

    for (auto p : globals_map) {
        if (p.second.empty())
            continue;

        Attribute attr = c->get_attribute(p.first);

        switch (attr.type()) {
        case CALI_TYPE_INT:
        case CALI_TYPE_BOOL:
        {
            std::vector<int> values(p.second.size());
            std::transform(p.second.begin(), p.second.end(), values.begin(), [](const Variant& v){
                    return v.to_int();
                });
            if (values.size() > 1)
                adiak::value(attr.name(), values);
            else
                adiak::value(attr.name(), values.front());
        }
        break;
        case CALI_TYPE_UINT:
        {
            std::vector<unsigned long> values(p.second.size());
            std::transform(p.second.begin(), p.second.end(), values.begin(), [](const Variant& v){
                    return static_cast<unsigned long>(v.to_uint());
                });
            if (values.size() > 1)
                adiak::value(attr.name(), values);
            else
                adiak::value(attr.name(), values.front());
        }
        break;
        default:
        {
            std::vector<std::string> values(p.second.size());
            std::transform(p.second.begin(), p.second.end(), values.begin(), [](const Variant& v){
                    return v.to_string();
                });
            if (values.size() > 1)
                adiak::value(attr.name(), values);
            else
                adiak::value(attr.name(), values.front());
        }
        }
    }
}

void
register_adiak_export(Caliper* c, Channel* chn)
{
    chn->events().pre_flush_evt.connect(
        [](Caliper* c, Channel* chn, const SnapshotRecord*){
            export_globals_to_adiak(c, chn);
        });
    
    Log(1).stream() << chn->name() << ": Registered adiak_export service" << std::endl;
}

} // namespace [anonymous]

namespace cali
{

CaliperService adiak_export_service { "adiak_export", ::register_adiak_export };

}
