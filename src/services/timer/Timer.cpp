// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Timestamp.cpp
// Timestamp provider for caliper records

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

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
        std::map<cali_id_t, std::vector<uint64_t>> inclusive_timer_stack;

        TimerInfo() : prev_snapshot_timestamp(0) {}
    };

    using clock = std::chrono::steady_clock;

    std::chrono::time_point<clock> tstart;

    Attribute timeoffs_attr;
    Attribute timerinfo_attr;

    Attribute snapshot_duration_attr;
    Attribute inclusive_duration_attr;
    Attribute offset_attr;

    // Keeps all created timer info objects so we can delete them later
    std::vector<TimerInfo*> info_obj_list;
    std::mutex              info_obj_mutex;

    bool record_inclusive_duration;

    Attribute begin_evt_attr;
    Attribute end_evt_attr;

    int n_stack_errors { 0 };

    TimerInfo* acquire_timerinfo(Caliper* c)
    {
        TimerInfo* ti = static_cast<TimerInfo*>(c->get(timerinfo_attr).value().get_ptr());

        if (!ti && !c->is_signal()) {
            ti = new TimerInfo;

            c->set(timerinfo_attr, Variant(cali_make_variant_from_ptr(ti)));

            std::lock_guard<std::mutex> g(info_obj_mutex);

            info_obj_list.push_back(ti);
        }

        return ti;
    }

    void snapshot_cb(Caliper* c, SnapshotView info, SnapshotBuilder& rec)
    {
        auto     now  = clock::now();
        uint64_t nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(now - tstart).count();

        rec.append(offset_attr, Variant(nsec));

        TimerInfo* ti = acquire_timerinfo(c);

        if (!ti)
            return;

        rec.append(snapshot_duration_attr, cali_make_variant_from_uint(nsec - ti->prev_snapshot_timestamp));
        ti->prev_snapshot_timestamp = nsec;

        if (record_inclusive_duration && !info.empty() && !c->is_signal()) {
            Attribute info_attr = c->get_attribute(info[0].attribute());
            Variant v_id = info_attr.get(begin_evt_attr);

            if (v_id) {
                // begin event: push current timestamp onto the inclusive timer stack
                ti->inclusive_timer_stack[v_id.to_id()].push_back(nsec);
            } else {
                v_id = info_attr.get(end_evt_attr);
                if (v_id) {
                    // end event: fetch begin timestamp from inclusive timer stack
                    auto stack_it = ti->inclusive_timer_stack.find(v_id.to_id());

                    if (stack_it == ti->inclusive_timer_stack.end() || stack_it->second.empty()) {
                        ++n_stack_errors;
                        return;
                    }

                    rec.append(inclusive_duration_attr, cali_make_variant_from_uint(nsec - stack_it->second.back()));
                    stack_it->second.pop_back();
                }
            }
        }
    }

    void post_init_cb(Caliper* c, Channel* chn)
    {
        // Find begin/end event snapshot event info attributes

        begin_evt_attr = c->get_attribute("cali.event.begin");
        end_evt_attr   = c->get_attribute("cali.event.end");

        if (!begin_evt_attr || !end_evt_attr) {
            if (record_inclusive_duration)
                Log(1).stream() << chn->name() << ": timer: Event attributes not found, disabling inclusive timers.\n";

            record_inclusive_duration = false;
        }

        // Initialize timer info on this thread
        acquire_timerinfo(c);
    }

    void finish_cb(Caliper*, Channel* chn)
    {
        if (n_stack_errors > 0)
            Log(1).stream() << chn->name() << ": timestamp: Encountered " << n_stack_errors
                            << " inclusive time stack errors!" << std::endl;
    }

    TimerService(Caliper* c, Channel* chn) : tstart(clock::now())
    {
        ConfigSet config          = services::init_config_from_spec(chn->config(), s_spec);
        record_inclusive_duration = config.get("inclusive_duration").to_bool();

        Attribute unit_attr = c->create_attribute("time.unit", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
        Variant   nsec_val  = Variant("nsec");

        offset_attr = c->create_attribute(
            "time.offset.ns",
            CALI_TYPE_UINT,
            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS,
            1,
            &unit_attr,
            &nsec_val
        );
        snapshot_duration_attr = c->create_attribute(
            "time.duration.ns",
            CALI_TYPE_UINT,
            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE,
            1,
            &unit_attr,
            &nsec_val
        );
        inclusive_duration_attr = c->create_attribute(
            "time.inclusive.duration.ns",
            CALI_TYPE_UINT,
            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE,
            1,
            &unit_attr,
            &nsec_val
        );
        timerinfo_attr = c->create_attribute(
            std::string("timer.info.") + std::to_string(chn->id()),
            CALI_TYPE_PTR,
            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_HIDDEN
        );
    }

    ~TimerService()
    {
        std::lock_guard<std::mutex> g(info_obj_mutex);

        for (TimerInfo* ti : info_obj_list)
            delete ti;
    }

public:

    static const char* s_spec;

    static void timer_register(Caliper* c, Channel* chn)
    {
        TimerService* instance = new TimerService(c, chn);

        chn->events().post_init_evt.connect([instance](Caliper* c, Channel* chn) { instance->post_init_cb(c, chn); });
        chn->events().create_thread_evt.connect([instance](Caliper* c, Channel*) { instance->acquire_timerinfo(c); });
        chn->events().snapshot.connect([instance](Caliper* c, SnapshotView info, SnapshotBuilder& rec) {
            instance->snapshot_cb(c, info, rec);
        });
        chn->events().finish_evt.connect([instance](Caliper* c, Channel* chn) {
            instance->finish_cb(c, chn);
            delete instance;
        });

        Log(1).stream() << chn->name() << ": Registered timer service" << std::endl;
    }

}; // class TimerService

const char* TimerService::s_spec = R"json(
{
"name": "timer",
"description": "Record timestamps and time durations",
"config":
[
 {
  "name": "inclusive_duration",
  "type": "bool",
  "description": "Record inclusive duration of begin/end regions",
  "value": "false"
 }
]}
)json";

const char* timestamp_spec = R"json(
{
"name": "timestamp",
"description": "Deprecated name for 'timer' service"
}
)json";

} // namespace

namespace cali
{

CaliperService timer_service     = { ::TimerService::s_spec, ::TimerService::timer_register };
CaliperService timestamp_service = { timestamp_spec, ::TimerService::timer_register };

} // namespace cali
