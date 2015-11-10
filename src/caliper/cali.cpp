/// \file cali.cpp
/// Caliper C interface implementation

#include "cali.h"

#include "Caliper.h"

#include <Variant.h>

#include <unordered_map>
#include <mutex>


using namespace cali;


namespace
{
    // Put attributes into a local map until we get fast by-id lookup in the runtime again

    typedef std::unordered_map<cali_id_t, Attribute> attr_map_t;

    attr_map_t attr_map;
    std::mutex attr_map_mutex;

    void 
    store_attribute(const Attribute& attr) {
        if (!(attr == Attribute::invalid)) {
            std::lock_guard<std::mutex> lock(attr_map_mutex);
            attr_map.insert(std::make_pair(attr.id(), attr));;
        }
    }

    Attribute 
    lookup_attribute(Caliper* c, cali_id_t attr_id) {
        Attribute attr { Attribute::invalid };

        std::lock_guard<std::mutex> lock(attr_map_mutex);
        auto f = attr_map.find(attr_id);

        if (f != attr_map.end()) { 
            attr = f->second;
        } else {
            attr = c->get_attribute(attr_id);

            if (!(attr == Attribute::invalid))
                attr_map.insert(std::make_pair(attr_id, attr));
        }

        return attr;
    }
}


//
// --- Attribute interface
//

cali_id_t 
cali_create_attribute(const char* name, cali_attr_type type, int properties)
{
    Attribute a = Caliper::instance()->create_attribute(name, type, properties);
    ::store_attribute(a);

    return a.id();
}

cali_id_t
cali_find_attribute(const char* name)
{
    Attribute a = Caliper::instance()->get_attribute(name);
    ::store_attribute(a);

    return a.id();
}


//
// --- Context interface
//

void
cali_push_context(int scope)
{
    Caliper::instance()->push_snapshot(scope);
}

//
// --- Environment interface
//

void*
cali_current_contextbuffer(cali_context_scope_t scope)
{
    return Caliper::instance()->current_contextbuffer(scope);
}

cali_err
cali_create_contextbuffer(cali_context_scope_t scope, void **new_env)
{
    *new_env = Caliper::instance()->create_contextbuffer(scope);
    return CALI_SUCCESS;
}


//
// --- Annotation interface
//

cali_err
cali_begin(cali_id_t attr_id, const void* value, size_t size)
{
    Caliper*     c = Caliper::instance();
    Attribute attr = ::lookup_attribute(c, attr_id);

    return c->begin(attr, Variant(attr.type(), value, size));
}

cali_err
cali_end(cali_id_t attr_id)
{
    Caliper*     c = Caliper::instance();
    Attribute attr = ::lookup_attribute(c, attr_id);

    return c->end(attr);
}

cali_err  
cali_set(cali_id_t attr_id, const void* value, size_t size)
{
    Caliper*     c = Caliper::instance();
    Attribute attr = ::lookup_attribute(c, attr_id);

    return c->set(attr, Variant(attr.type(), value, size));
}

