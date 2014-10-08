/// @file AttributeStore.cpp
/// AttributeStore class implementation

#include "AttributeStore.h"

#include "Reader.h"
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

    void read(AttributeReader& r) {
        attributes.clear();
        namelist.clear();

        for (AttributeReader::AttributeInfo info = r.read(); info.id != CTX_INV_ID; info = r.read()) {
            if (attributes.size() < info.id)
                attributes.reserve(info.id);

            attributes[info.id] = Attribute(info.id, info.name, info.type, info.properties);
            namelist.insert(make_pair(info.name, info.id));
        }
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

Attribute AttributeStore::get(ctx_id_t id) const
{
    mP->lock.rlock();
    auto p = mP->get(id);
    mP->lock.unlock();

    return p;
}

Attribute AttributeStore::get(const std::string& name) const
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

void AttributeStore::foreach_attribute(std::function<void(const Attribute&)> proc) const
{
    mP->lock.rlock();
    mP->foreach_attribute(proc);
    mP->lock.unlock();
}

void AttributeStore::read(AttributeReader& r)
{
    mP->lock.wlock();
    mP->read(r);
    mP->lock.unlock();
}
