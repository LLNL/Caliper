/// @cali.cpp
/// Caliper C interface implementation

#include "cali.h"

#include "Caliper.h"

using namespace cali;

//
// --- Attribute interface
//

cali_id_t 
cali_create_attribute(const char* name, cali_attr_type type, cali_attr_properties properties)
{
    Attribute a = Caliper::instance()->create_attribute(name, type, properties);
    return a.id();
}

cali_id_t
cali_find_attribute(const char* name)
{
    Attribute a = Caliper::instance()->get_attribute(name);
    return a.id();
}

//
// --- Context interface
//

size_t
cali_get_context_size(cali_id_t env)
{
    return Caliper::instance()->context_size(env);
}

size_t
cali_get_context(cali_id_t env, uint64_t* buf, size_t bufsize)
{
    return Caliper::instance()->get_context(env, buf, bufsize);
}

//
// --- Environment interface
//

cali_id_t
cali_get_environment()
{
    return Caliper::instance()->current_environment();
}

//
// --- Annotation interface
//

cali_err
cali_begin(cali_id_t env, cali_id_t attr, const void* value, size_t size)
{
    Caliper* c = Caliper::instance();
    return c->begin(env, c->get_attribute(attr), value, size);
}

cali_err
cali_end(cali_id_t env, cali_id_t attr)
{
    Caliper* c = Caliper::instance();
    return c->end(env, c->get_attribute(attr));
}

cali_err  
cali_set(cali_id_t env, cali_id_t attr, const void* value, size_t size)
{
    Caliper* c = Caliper::instance();
    return c->set(env, c->get_attribute(attr), value, size);
}
