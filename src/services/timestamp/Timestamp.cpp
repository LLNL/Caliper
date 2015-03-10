///@file  Timestamp.cpp
///@brief Timestamp provider for caliper records

#include "../CaliperService.h"

#include <Caliper.h>

#include <ContextRecord.h>
#include <Log.h>

#include <chrono>
#include <string>
#include <type_traits>

using namespace cali;
using namespace std;

namespace 
{

Attribute attribute { Attribute::invalid } ;
chrono::time_point<chrono::high_resolution_clock> tstart;

void push_time(int scope, WriteRecordFn fn) {
    if (scope & CALI_SCOPE_PROCESS) {
        auto now = chrono::high_resolution_clock::now();
        uint64_t usec = chrono::duration_cast<chrono::microseconds>(now - tstart).count();

        Variant v_attr(attribute.id());
        Variant v_time(usec);

        int               n[3] = { 0, 1, 1 };
        const Variant* data[3] = { nullptr, &v_attr, &v_time };

        fn(ContextRecord::record_descriptor(), n, data);
    }
}

/// Initialization handler
void timestamp_register(Caliper* c)
{
    // set start time and create time attribute
    tstart    = chrono::high_resolution_clock::now();
    attribute = 
        c->create_attribute("timestamp(usec)", CALI_TYPE_UINT, 
                            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_SKIP_EVENTS);

    // add callback for Caliper::get_context() event
    c->events().measure.connect(&push_time);

    Log(1).stream() << "Registered timestamp service" << endl;
}

} // namespace


namespace cali
{
    CaliperService TimestampService = { "timestamp", { ::timestamp_register } };
} // namespace cali
