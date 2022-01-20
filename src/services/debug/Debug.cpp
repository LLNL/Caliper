// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Debug.cpp
// Caliper debug service

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"

#include <mutex>

using namespace cali;
using namespace std;

namespace
{

mutex dbg_mutex;

void create_attr_cb(Caliper* c, Channel* chn, const Attribute& attr)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: create_attribute (attr=" << attr << ")" << endl;
}

void begin_cb(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: pre_begin (" << attr.name() << "=" << value << ")" << endl;
}

void end_cb(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: pre_end ("   << attr.name() << "=" << value << ")" << endl;
}

void set_cb(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: pre_set ("   << attr.name() << "=" << value << ")" << endl;
}

const char* scopestrings[] = { "", "process", "thread", "", "task" };

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

void create_thread_cb(Caliper* c, Channel* chn)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: create_thread" << endl;
}

void release_thread_cb(Caliper* c, Channel* chn)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: release_thread" << endl;
}

void snapshot_cb(Caliper* c, Channel* chn, int scope, SnapshotView, SnapshotBuilder&)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: snapshot (scope=" << scope2string(scope) << ", "
                    << ")" << endl;
}

std::ostream& print_snapshot_record(Caliper* c, std::ostream& os, SnapshotView rec)
{
    os << "{ ";

    int count = 0;
    for (const Entry& e : rec) {
        os << (count++ > 0 ? ", " : "");
        if (e.is_reference())
            os << e.node()->id();
        else
            os << c->get_attribute(e.attribute()).name_c_str() << ": " << e.value().to_string();
    }

    return (os << " }");
}
    
void process_snapshot_cb(Caliper* c, Channel* chn, SnapshotView, SnapshotView rec)
{
    lock_guard<mutex> lock(dbg_mutex);    

    print_snapshot_record( c, Log(1).stream() << chn->name() << ": Event: process_snapshot: ", rec ) << std::endl;
}

void finish_cb(Caliper* c, Channel* chn)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: finish" << endl;
}

void debug_service_register(Caliper* c, Channel* chn)
{
    chn->events().create_attr_evt.connect(&create_attr_cb);
    chn->events().pre_begin_evt.connect(&begin_cb);
    chn->events().pre_end_evt.connect(&end_cb);
    chn->events().pre_set_evt.connect(&set_cb);
    chn->events().finish_evt.connect(&finish_cb);
    chn->events().create_thread_evt.connect(&create_thread_cb);
    chn->events().release_thread_evt.connect(&release_thread_cb);
    chn->events().snapshot.connect(&snapshot_cb);
    chn->events().process_snapshot.connect(&process_snapshot_cb);

    Log(1).stream() << chn->name() << ": Registered debug service" << endl;
}

} // namespace

namespace cali
{

CaliperService debug_service = { "debug", ::debug_service_register };

}
