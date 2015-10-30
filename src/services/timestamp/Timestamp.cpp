///@file  Timestamp.cpp
///@brief Timestamp provider for caliper records

#include "../CaliperService.h"

#include <Caliper.h>
#include <SigsafeRWLock.h>
#include <Snapshot.h>

#include <RuntimeConfig.h>
#include <ContextRecord.h>
#include <Log.h>

#include <chrono>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

using namespace cali;
using namespace std;

namespace 
{

chrono::time_point<chrono::high_resolution_clock> tstart;

Attribute timestamp_attr { Attribute::invalid } ;
Attribute timeoffs_attr  { Attribute::invalid } ;
Attribute snapshot_duration_attr { Attribute::invalid };
Attribute phase_duration_attr    { Attribute::invalid };

ConfigSet config;

bool      record_timestamp;
bool      record_offset;
bool      record_duration;
bool      record_phases;

Attribute begin_evt_attr { Attribute::invalid };
Attribute end_evt_attr   { Attribute::invalid };

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
    { "phases", CALI_TYPE_BOOL, "true",
      "Record begin/end phase durations.",
      "Record begin/end phase durations."
    },
    ConfigSet::Terminator
};


void snapshot_cb(Caliper* c, int scope, const Caliper::Entry* entry_info, Snapshot* sbuf) {
    Snapshot::Sizes     sizes = sbuf->capacity();
    Snapshot::Addresses addr  = sbuf->addresses();

    auto now = chrono::high_resolution_clock::now();

    int capacity = std::min(sizes.n_attr, sizes.n_data);

    sizes = { 0, 0, 0 };

    if ((record_duration || record_phases || record_offset) && scope & CALI_SCOPE_THREAD) {
        uint64_t  usec = chrono::duration_cast<chrono::microseconds>(now - tstart).count();
        Variant v_offs = c->exchange(timeoffs_attr, Variant(usec));

        if (capacity-- > 0 && record_duration && !v_offs.empty()) {
            uint64_t duration = usec - v_offs.to_uint();

            addr.immediate_attr[sizes.n_attr++] = snapshot_duration_attr.id();
            addr.immediate_data[sizes.n_data++] = Variant(duration);
        }

        if (record_phases) {
            cali_id_t evt_attr_id = c->get_entry_attribute_id(entry_info);

            if        (evt_attr_id == begin_evt_attr.id()) {
                entry_info->ref()->data().to_id();
            } else if (evt_attr_id == end_evt_attr.id())   {
            }
        }
    }

    if (capacity-- > 0 && record_timestamp && (scope & CALI_SCOPE_PROCESS)) {
        addr.immediate_attr[sizes.n_attr++] = timestamp_attr.id();
        addr.immediate_data[sizes.n_data++] = Variant(static_cast<int>(chrono::system_clock::to_time_t(chrono::system_clock::now())));
    }

    sbuf->commit(sizes);
}

void post_init_cb(Caliper* c)
{
    // Find begin/end event snapshot event info attributes

    begin_evt_attr = c->get_attribute("cali.snapshot.event.begin");
    end_evt_attr   = c->get_attribute("cali.snapshot.event.end");

    if (begin_evt_attr == Attribute::invalid || end_evt_attr == Attribute::invalid) {
        if (record_phases)
            Log(1).stream() << "Warning: \"event\" service with snapshot info\n"
                "    is required for phase timers." << std::endl;

        record_phases = false;
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
    record_phases    = config.get("phases").to_bool();

    int hide_offset  = (record_duration && !record_offset ? CALI_ATTR_HIDDEN : 0);

    timestamp_attr = 
        c->create_attribute("time.timestamp", CALI_TYPE_UINT, 
                            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_SKIP_EVENTS);
    timeoffs_attr = 
        c->create_attribute("time.end.offset", CALI_TYPE_UINT, 
                            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD  | CALI_ATTR_SKIP_EVENTS | hide_offset);
    snapshot_duration_attr = 
        c->create_attribute("time.duration", CALI_TYPE_UINT, 
                            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD  | CALI_ATTR_SKIP_EVENTS);
    phase_duration_attr = 
        c->create_attribute("time.phase_duration", CALI_TYPE_UINT, 
                            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD  | CALI_ATTR_SKIP_EVENTS);

    c->set(timeoffs_attr, Variant(static_cast<uint64_t>(0)));

    // c->events().create_attr_evt.connect(&create_attr_cb);
    c->events().post_init_evt.connect(&post_init_cb);
    c->events().snapshot.connect(&snapshot_cb);

    Log(1).stream() << "Registered timestamp service" << endl;
}

} // namespace


namespace cali
{
    CaliperService TimestampService = { "timestamp", ::timestamp_service_register };
} // namespace cali
