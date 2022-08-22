// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Timestamp.cpp
// Timestamp provider for caliper records

#include "caliper/CaliperService.h"

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/Log.h"

#include <cassert>
#include <chrono>
#include <map>
#include <mutex>
#include <ratio>
#include <string>
#include <type_traits>
#include <vector>

using namespace cali;
using namespace std;

namespace
{

class TimerService
{
    //   This keeps per-thread per-channel timer data, which we can look up
    // on the thread-local blackboard
    struct TimerInfo {
        // The timestamp of the last snapshot on this channel+thread
        uint64_t prev_snapshot_timestamp;

        // A per-attribute stack of timestamps for computing inclusive times
        std::map< cali_id_t, std::vector<uint64_t> > inclusive_timer_stack;

        TimerInfo()
            : prev_snapshot_timestamp(0)
        { }
    };

    chrono::time_point<chrono::high_resolution_clock> tstart;

    Attribute timestamp_attr { Attribute::invalid } ;
    Attribute timeoffs_attr  { Attribute::invalid } ;

    Attribute timerinfo_attr { Attribute::invalid } ;

    Attribute snapshot_duration_attr;
    Attribute inclusive_duration_attr;
    Attribute offset_attr;

    // Keeps all created timer info objects so we can delete them later
    std::vector<TimerInfo*>  info_obj_list;
    std::mutex               info_obj_mutex;

    bool      record_timestamp;
    bool      record_offset;
    bool      record_snapshot_duration;
    bool      record_inclusive_duration;

    double    scale_factor   { 1.0 };

    Attribute begin_evt_attr { Attribute::invalid };
    Attribute end_evt_attr   { Attribute::invalid };

    int       n_stack_errors { 0 };

    TimerInfo* acquire_timerinfo(Caliper* c) {
        TimerInfo* ti =
            static_cast<TimerInfo*>(c->get(timerinfo_attr).value().get_ptr());

        if (!ti && !c->is_signal()) {
            ti = new TimerInfo;

            c->set(timerinfo_attr, Variant(cali_make_variant_from_ptr(ti)));

            std::lock_guard<std::mutex>
                g(info_obj_mutex);

            info_obj_list.push_back(ti);
        }

        return ti;
    }

    void snapshot_cb(Caliper* c, Channel* chn, int scope, SnapshotView info, SnapshotBuilder& rec) {
        auto now = chrono::high_resolution_clock::now();
        uint64_t usec = chrono::duration_cast<chrono::microseconds>(now - tstart).count();

        if (record_offset)
            rec.append(offset_attr, Variant(usec));

        if (record_timestamp && (scope & CALI_SCOPE_PROCESS)) {
            auto timestamp =
                chrono::duration_cast<chrono::nanoseconds>(now.time_since_epoch()).count();
            rec.append(timestamp_attr, cali_make_variant_from_uint(timestamp));
        }

        if ((!record_snapshot_duration && !record_inclusive_duration) || !(scope & CALI_SCOPE_THREAD))
            return;

        // Get timer info object for this thread and channel
        TimerInfo* ti = acquire_timerinfo(c);

        if (!ti)
            return;

        if (record_snapshot_duration)
            rec.append(snapshot_duration_attr, Variant(scale_factor * (usec - ti->prev_snapshot_timestamp)));

        ti->prev_snapshot_timestamp = usec;

        if (record_inclusive_duration && !info.empty() && !c->is_signal()) {
            Entry event = info.get(begin_evt_attr);

            if (event.empty())
                event = info.get(end_evt_attr);
            if (event.empty())
                return;

            cali_id_t evt_attr_id = event.value().to_id();

            if (event.attribute() == begin_evt_attr.id()) {
                // begin event: push current timestamp onto the inclusive timer stack
                ti->inclusive_timer_stack[evt_attr_id].push_back(usec);
            } else if (event.attribute() == end_evt_attr.id()) {
                // end event: fetch begin timestamp from inclusive timer stack
                auto stack_it = ti->inclusive_timer_stack.find(evt_attr_id);

                if (stack_it == ti->inclusive_timer_stack.end() || stack_it->second.empty()) {
                    ++n_stack_errors;
                    return;
                }

                rec.append(inclusive_duration_attr, Variant(scale_factor * (usec - stack_it->second.back())));
                stack_it->second.pop_back();
            }
        }
    }

    void post_init_cb(Caliper* c, Channel* chn) {
        // Find begin/end event snapshot event info attributes

        begin_evt_attr = c->get_attribute("cali.event.begin");
        end_evt_attr   = c->get_attribute("cali.event.end");

        if (begin_evt_attr == Attribute::invalid || end_evt_attr == Attribute::invalid) {
            if (record_inclusive_duration)
                Log(1).stream() << chn->name() << ": Timestamp: Note: event trigger attributes not registered,\n"
                    "    disabling phase timers." << std::endl;

            record_inclusive_duration = false;
        }

        // Initialize timer info on this thread
        acquire_timerinfo(c);
    }

    void finish_cb(Caliper*, Channel* chn) {
        if (n_stack_errors > 0)
            Log(1).stream() << chn->name() << ": timestamp: Encountered "
                            << n_stack_errors
                            << " inclusive time stack errors!"
                            << std::endl;
    }

    TimerService(Caliper* c, Channel* chn)
        : tstart(chrono::high_resolution_clock::now())
        {
            ConfigSet config = services::init_config_from_spec(chn->config(), s_spec);

            record_snapshot_duration =
                config.get("snapshot_duration").to_bool();
            record_offset    =
                config.get("offset").to_bool();
            record_timestamp =
                config.get("timestamp").to_bool();
            record_inclusive_duration
                = config.get("inclusive_duration").to_bool();

            Attribute unit_attr =
                c->create_attribute("time.unit", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);

            Variant   usec_val  = Variant("usec");
            Variant   sec_val   = Variant("sec");

            Variant   unit_val  = usec_val;

            std::string unitstr = config.get("unit").to_string();

            if (unitstr == "sec") {
                unit_val = sec_val;
                scale_factor = 1e-6;
            } else if (unitstr != "usec")
                Log(0).stream() << chn->name() << ": timestamp: Unknown unit " << unitstr
                                << std::endl;

            timestamp_attr =
                c->create_attribute("time.timestamp", CALI_TYPE_UINT,
                                    CALI_ATTR_ASVALUE       |
                                    CALI_ATTR_SCOPE_PROCESS |
                                    CALI_ATTR_SKIP_EVENTS,
                                    1, &unit_attr, &sec_val);
            offset_attr =
                c->create_attribute("time.offset",    CALI_TYPE_UINT,
                                    CALI_ATTR_ASVALUE       |
                                    CALI_ATTR_SCOPE_THREAD  |
                                    CALI_ATTR_SKIP_EVENTS,
                                    1, &unit_attr, &usec_val);
            snapshot_duration_attr =
                c->create_attribute("time.duration",  CALI_TYPE_DOUBLE,
                                    CALI_ATTR_ASVALUE       |
                                    CALI_ATTR_SCOPE_THREAD  |
                                    CALI_ATTR_SKIP_EVENTS   |
                                    CALI_ATTR_AGGREGATABLE,
                                    1, &unit_attr, &unit_val);
            inclusive_duration_attr =
                c->create_attribute("time.inclusive.duration", CALI_TYPE_DOUBLE,
                                    CALI_ATTR_ASVALUE       |
                                    CALI_ATTR_SCOPE_THREAD  |
                                    CALI_ATTR_SKIP_EVENTS   |
                                    CALI_ATTR_AGGREGATABLE,
                                    1, &unit_attr, &unit_val);
            timerinfo_attr =
                c->create_attribute(std::string("timer.info.") + std::to_string(chn->id()), CALI_TYPE_PTR,
                                    CALI_ATTR_ASVALUE       |
                                    CALI_ATTR_SCOPE_THREAD  |
                                    CALI_ATTR_SKIP_EVENTS   |
                                    CALI_ATTR_HIDDEN);
        }

        ~TimerService() {
            std::lock_guard<std::mutex>
                g(info_obj_mutex);

            for (TimerInfo* ti : info_obj_list)
                delete ti;
        }

public:

    static const char* s_spec;

    static void timer_register(Caliper* c, Channel* chn) {
        TimerService* instance = new TimerService(c, chn);

        chn->events().post_init_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->post_init_cb(c, chn);
            });
        chn->events().create_thread_evt.connect(
            [instance](Caliper* c, Channel*){
                instance->acquire_timerinfo(c);
            });
        chn->events().snapshot.connect(
            [instance](Caliper* c, Channel* chn, int scopes, SnapshotView info, SnapshotBuilder& rec){
                instance->snapshot_cb(c, chn, scopes, info, rec);
            });
        chn->events().finish_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->finish_cb(c, chn);
                delete instance;
            });

        Log(1).stream() << chn->name() << ": Registered timer service" << endl;
    }

}; // class TimerService

const char* TimerService::s_spec = R"json(
{   "name": "timer",
    "description": "Record timestamps and time durations",
    "config": [
        {   "name": "snapshot_duration",
            "description": "Include duration of snapshot epoch with each snapshot record",
            "type": "bool",
            "value": "true"
        },
        {   "name": "offset",
            "description": "Include timestamp (time since program start) in snapshots",
            "type": "bool",
            "value": "false"
        },
        {   "name": "timestamp",
            "description": "Include absolute timestamp in POSIX time",
            "type": "bool",
            "value": "false"
        },
        {   "name": "inclusive_duration",
            "description": "Record inclusive duration of begin/end regions",
            "type": "bool",
            "value": "true"
        },
        {   "name": "unit",
            "description": "Unit for time durations (sec, usec)",
            "type": "string",
            "value": "sec"
        }
    ]
}
)json";

const char* timestamp_spec = R"json(
{   "name": "timestamp",
    "description": "Deprecated name for 'timer' service"
}
)json";

}  // namespace


namespace cali
{

CaliperService timer_service = { ::TimerService::s_spec, ::TimerService::timer_register };
CaliperService timestamp_service = { timestamp_spec, ::TimerService::timer_register };

} // namespace cali
