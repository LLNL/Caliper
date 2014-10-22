/// @file AttributeStore.cpp
/// AttributeStore class implementation

#include "AttributeStore.h"

#include <map>
#include <vector>

using namespace cali;
using namespace std;

struct AttributeStore::AttributeStoreImpl
{
    // --- Data
    
    vector<Attribute>     attributes;
    map<string, ctx_id_t> namelist;


    Attribute create(const std::string&  name, ctx_attr_type type, int properties) {
        auto it = namelist.find(name);

        if (it != namelist.end())
            return attributes[it->second];

        ctx_id_t id = attributes.size();

        namelist.insert(make_pair(name, id));
        attributes.push_back(Attribute(id, name, type, properties));

        return attributes.back();
    }

    Attribute get(ctx_id_t id) const {
        if (id < attributes.size())
            return attributes[id];

        return Attribute::invalid;
    }

    Attribute get(const std::string& name) const {
        auto it = namelist.find(name);

        if (it == namelist.end())
            return Attribute::invalid;

        return attributes[it->second];
    }

    void foreach_attribute(std::function<void(const Attribute&)> proc) const {
        for ( const Attribute& a : attributes )
            proc(a);
    }

    // void read(AttributeReader& r) {
    //     attributes.clear();
    //     namelist.clear();

    //     for (AttributeReader::AttributeInfo info = r.read(); info.id != CTX_INV_ID; info = r.read()) {
    //         if (attributes.size() < info.id)
    //             attributes.reserve(info.id);

    //         attributes[info.id] = Attribute(info.id, info.name, info.type, info.properties);
    //         namelist.insert(make_pair(info.name, info.id));
    //     }
    // }
};


//
// --- AttributeStore interface
//

AttributeStore::AttributeStore()
    : mP { new AttributeStoreImpl }
{ 
}

AttributeStore::~AttributeStore()
{
    mP.reset();
}

Attribute AttributeStore::get(ctx_id_t id) const
{
    return mP->get(id);
}

Attribute AttributeStore::get(const std::string& name) const
{
    return mP->get(name);;
}

Attribute AttributeStore::create(const std::string& name, ctx_attr_type type, int properties)
{
    return mP->create(name, type, properties);
}

void AttributeStore::foreach_attribute(std::function<void(const Attribute&)> proc) const
{
    mP->foreach_attribute(proc);
}

// void AttributeStore::read(AttributeReader& r)
// {
//     mP->read(r);
// }
