/// @file  Debug.cpp
/// @brief Caliper debug service

#include "../CaliperService.h"

#include <Caliper.h>

#include <Log.h>

using namespace cali;
using namespace std;

namespace
{

void create_attr_cb(Caliper* c, const Attribute& attr)
{
    Log(2).stream() << "Event: create_attribute (attr = " << attr.name() << ")" << endl;
}

void begin_cb(Caliper* c, cali_id_t env, const Attribute& attr)
{
    Log(2).stream() << "Event: begin (env = " << env << ", attr = " << attr.name() << ")" << endl;
}

void end_cb(Caliper* c, cali_id_t env, const Attribute& attr)
{
    Log(2).stream() << "Event: end (env = " << env << ", attr = " << attr.name() << ")" << endl;
}

void set_cb(Caliper* c, cali_id_t env, const Attribute& attr)
{
    Log(2).stream() << "Event: set (env = " << env << ", attr = " << attr.name() << ")" << endl;
}

const char* scopestrings[] = { "process", "thread", "task" };

void query_cb(Caliper* c, cali_context_scope_t scope)
{
    cali_id_t env = c->current_environment(scope);

    Log(2).stream() 
        << "Event: get_context (scope = " << scopestrings[scope] 
        << ", env = " << env
        << ")" << endl;
}

void try_query_cb(Caliper* c, cali_context_scope_t scope)
{
    cali_id_t env = c->current_environment(scope);

    Log(2).stream() 
        << "Event: try_get_context (scope = " << scopestrings[scope] 
        << ", env = " << env
        << ")" << endl;
}

void finish_cb(Caliper* c)
{
    Log(2).stream() << "Event: finish" << endl;
}

void debug_register(Caliper* c)
{
    c->events().createAttrEvt.connect(&create_attr_cb);
    c->events().beginEvt.connect(&begin_cb);
    c->events().endEvt.connect(&end_cb);
    c->events().setEvt.connect(&set_cb);
    c->events().queryEvt.connect(&query_cb);
    c->events().tryQueryEvt.connect(&try_query_cb);
    c->events().finishEvt.connect(&finish_cb);

    Log(1).stream() << "Registered debug service" << endl;
}

} // namespace

namespace cali
{
    CaliperService DebugService = { "debug", ::debug_register };
}
