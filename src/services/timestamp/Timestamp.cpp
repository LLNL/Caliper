///@file  Timestamp.cpp
///@brief Timestamp provider for caliper records

#include "../CaliperService.h"

#include <Caliper.h>

#include <RuntimeConfig.h>
#include <ContextRecord.h>
#include <Log.h>

#include <chrono>
#include <string>
#include <type_traits>

using namespace cali;
using namespace std;

namespace 
{

chrono::time_point<chrono::high_resolution_clock> tstart;

Attribute timestamp_attr   { Attribute::invalid } ;
Attribute timeoffs_attr    { Attribute::invalid } ;
Attribute duration_attr    { Attribute::invalid } ;

ConfigSet config;

bool      record_timestamp;
bool      record_offset;
bool      record_duration;

static const ConfigSet::Entry s_configdata[] = {
    { "duration", CALI_TYPE_BOOL, "true",
      "Include duration of context epoch with each context record",
      "Include duration of context epoch with each context record"
    },
    { "offset", CALI_TYPE_BOOL, "false",
      "Include time offset (time since program start) with each context record",
      "Include time offset (time since program start) with each context record"
    },
    { "timestamp", CALI_TYPE_BOOL, "false",
      "Include absolute timestamp (POSIX time) with each context record",
      "Include absolute timestamp (POSIX time) with each context record"
    },
    ConfigSet::Terminator
};

void push_time(Caliper* c, int scope, WriteRecordFn fn) {
    auto now = chrono::high_resolution_clock::now();

    Variant v_attr[2];
    Variant v_data[2];

    int count = 0;

    if ((record_duration || record_offset) && scope & CALI_SCOPE_THREAD) {
        uint64_t  usec = chrono::duration_cast<chrono::microseconds>(now - tstart).count();
        Variant v_offs = c->exchange(timeoffs_attr, Variant(usec));

        if (record_duration && !v_offs.empty()) {
            uint64_t duration = usec - v_offs.to_uint();

            v_attr[count] = Variant(duration_attr.id());
            v_data[count] = Variant(duration);

            ++count;
        }
    }

    if (record_timestamp && (scope & CALI_SCOPE_PROCESS)) {
        v_attr[count] = Variant(timestamp_attr.id());
        v_data[count] = Variant(static_cast<int>(chrono::system_clock::to_time_t(chrono::system_clock::now())));

        ++count;
    }

    if (count) {
        int               n[3] = { 0,       count,  count  };
        const Variant* data[3] = { nullptr, v_attr, v_data };

        fn(ContextRecord::record_descriptor(), n, data);
    }
}

/// Initialization handler
void timestamp_service_register(Caliper* c)
{
    // set start time and create time attribute
    tstart = chrono::high_resolution_clock::now();

    config = RuntimeConfig::init("timer", s_configdata);

    record_duration  = config.get("duration").to_bool();
    record_offset    = config.get("offset").to_bool();
    record_timestamp = config.get("timestamp").to_bool();

    int hide_offset  = (record_duration && !record_offset ? CALI_ATTR_HIDDEN : 0);

    timestamp_attr = 
        c->create_attribute("time.timestamp", CALI_TYPE_UINT, 
                            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_SKIP_EVENTS);
    timeoffs_attr = 
        c->create_attribute("time.end.offset", CALI_TYPE_UINT, 
                            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD  | CALI_ATTR_SKIP_EVENTS | hide_offset);
    duration_attr = 
        c->create_attribute("time.duration", CALI_TYPE_UINT, 
                            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD  | CALI_ATTR_SKIP_EVENTS);

    c->set(timeoffs_attr, Variant(static_cast<uint64_t>(0)));

    // add callback for Caliper::get_context() event
    c->events().measure.connect(&push_time);

    Log(1).stream() << "Registered timestamp service" << endl;
}

} // namespace


namespace cali
{
    CaliperService TimestampService = { "timestamp", ::timestamp_service_register };
} // namespace cali
