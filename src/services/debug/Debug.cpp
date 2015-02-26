/// @file  Debug.cpp
/// @brief Caliper debug service

#include "../CaliperService.h"

#include <Caliper.h>

#include <Log.h>

#include <mutex>

using namespace cali;
using namespace std;

namespace
{

mutex dbg_mutex;

void create_attr_cb(Caliper* c, const Attribute& attr)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(2).stream() << "Event: create_attribute (attr = " << attr.record() << ")" << endl;
}

void begin_cb(Caliper* c, const Attribute& attr)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(2).stream() << "Event: begin (attr = " << attr.name() << ")" << endl;
}

void end_cb(Caliper* c, const Attribute& attr)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(2).stream() << "Event: end (attr = " << attr.name() << ")" << endl;
}

void set_cb(Caliper* c, const Attribute& attr)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(2).stream() << "Event: set (attr = " << attr.name() << ")" << endl;
}

const char* scopestrings[] = { "process", "thread", "task" };

string scope2string(int scope)
{
    const cali_context_scope_t scopes[] = { 
        CALI_SCOPE_TASK, CALI_SCOPE_THREAD, CALI_SCOPE_PROCESS 
    };

    string out;

    for (cali_context_scope_t s : scopes)
        if (scope & s) {
            if (out.size())
                out.append(":");
            out.append(scopestrings[s]);
        }

    return out;
}

void query_cb(Caliper* c, int scope)
{
    lock_guard<mutex> lock(dbg_mutex);

    Log(2).stream() 
        << "Event: get_context (scope = " << scope2string(scope) 
        << ")" << endl;
}

void try_query_cb(Caliper* c, int scope)
{
    lock_guard<mutex> lock(dbg_mutex);

    Log(2).stream() 
        << "Event: try_get_context (scope = " << scope2string(scope)
        << ")" << endl;
}

void finish_cb(Caliper* c)
{
    lock_guard<mutex> lock(dbg_mutex);
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
