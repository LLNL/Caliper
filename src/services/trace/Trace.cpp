/// @file  Trace.cpp
/// @brief Caliper trace service

#include "../CaliperService.h"

#include <Caliper.h>
#include <Snapshot.h>

#include <Log.h>

using namespace cali;
using namespace std;


namespace 
{    

void process_snapshot_cb(Caliper* c, const Snapshot* sbuf)
{
    sbuf->push_record(c->events().write_record);
}

void trace_register(Caliper* c)
{
    c->events().process_snapshot.connect(&process_snapshot_cb);

    Log(1).stream() << "Registered trace service" << endl;
}

} // namespace

namespace cali
{
    CaliperService TraceService { "trace", &::trace_register };
}
