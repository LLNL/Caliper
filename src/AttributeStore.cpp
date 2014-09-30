/// @file AttributeStore.cpp
/// AttributeStore class implementation

#include "AttributeStore.h"
#include "SigsafeRWLock.h"

#include <map>
#include <vector>

using namespace cali;
using namespace std;

struct AttributeStore::AttributeStoreImpl
{
    // --- Data
    
    mutable SigsafeRWLock lock;

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

    pair<bool, Attribute> get(ctx_id_t id) const {
        if (id < attributes.size())
            return make_pair(true, attributes[id]);

        return make_pair(false, Attribute::invalid);
    }

    pair<bool, Attribute> get(const std::string& name) const {
        auto it = namelist.find(name);

        if (it == namelist.end())
            return make_pair(false, Attribute::invalid);

        return make_pair(true, attributes[it->second]);
    }
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

pair<bool, Attribute> AttributeStore::get(ctx_id_t id) const
{
    mP->lock.rlock();
    auto p = mP->get(id);
    mP->lock.unlock();

    return p;
}

pair<bool, Attribute> AttributeStore::get(const std::string& name) const
{
    mP->lock.rlock();
    auto p = mP->get(name);
    mP->lock.unlock();

    return p;
}

Attribute AttributeStore::create(const std::string& name, ctx_attr_type type, int properties)
{
    mP->lock.wlock();
    Attribute attr = mP->create(name, type, properties);
    mP->lock.unlock();

    return attr;
}
