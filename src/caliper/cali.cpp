/// @cali.cpp
/// Caliper C interface implementation

#include "cali.h"

#include "Caliper.h"

#include <Variant.h>

using namespace cali;

//
// --- Attribute interface
//

cali_id_t 
cali_create_attribute(const char* name, cali_attr_type type, int properties)
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

void
cali_push_context(int scope)
{
    Caliper::instance()->push_snapshot(scope, nullptr);
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
    Attribute attr = c->get_attribute(attr_id);
    return c->begin(attr, Variant(attr.type(), value, size));
}

cali_err
cali_end(cali_id_t attr_id)
{
    Caliper* c = Caliper::instance();
    return c->end(c->get_attribute(attr_id));
}

cali_err  
cali_set(cali_id_t attr_id, const void* value, size_t size)
{
    Caliper*     c = Caliper::instance();
    Attribute attr = c->get_attribute(attr_id);
    return c->set(attr, Variant(attr.type(), value, size));
}

//
// --- I/O interface
//

// cali_err
// cali_write_metadata(void)
// {
//     if (Caliper::instance()->write_metadata())
//         return CALI_SUCCESS;

//     return CALI_EINV;
// }
