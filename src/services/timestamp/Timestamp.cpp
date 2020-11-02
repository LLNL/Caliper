// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Timestamp.cpp
// Timestamp provider for caliper records

#include "caliper/CaliperService.h"

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

class Timestamp
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

    cali_id_t snapshot_duration_attr_id  { CALI_INV_ID };
    cali_id_t inclusive_duration_attr_id { CALI_INV_ID };
    cali_id_t offset_attr_id             { CALI_INV_ID };

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

    static const ConfigSet::Entry s_configdata[];


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

    void snapshot_cb(Caliper* c, Channel* chn, int scope, const SnapshotRecord* info, SnapshotRecord* rec) {
        auto now = chrono::high_resolution_clock::now();
        uint64_t usec = chrono::duration_cast<chrono::microseconds>(now - tstart).count();

        if (record_offset)
            rec->append(offset_attr_id, Variant(usec));

        if (record_timestamp && (scope & CALI_SCOPE_PROCESS))
            rec->append(timestamp_attr.id(),
                        Variant(cali_make_variant_from_uint(
                                chrono::duration_cast<chrono::nanoseconds>(chrono::system_clock::now().time_since_epoch()).count())));

        if ((!record_snapshot_duration && !record_inclusive_duration) || !(scope & CALI_SCOPE_THREAD))
            return;

        // Get timer info object for this thread and channel
        TimerInfo* ti = acquire_timerinfo(c);

        if (!ti)
            return;

        if (record_snapshot_duration)
            rec->append(snapshot_duration_attr_id, Variant(scale_factor * (usec - ti->prev_snapshot_timestamp)));

        ti->prev_snapshot_timestamp = usec;

        if (record_inclusive_duration && info && !c->is_signal()) {
            Entry event = info->get(begin_evt_attr);

            if (event.is_empty())
                event = info->get(end_evt_attr);
            if (event.is_empty())
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

                rec->append(inclusive_duration_attr_id, Variant(scale_factor * (usec - stack_it->second.back())));
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

    Timestamp(Caliper* c, Channel* chn)
        : tstart(chrono::high_resolution_clock::now())
        {
            ConfigSet config = chn->config().init("timer", s_configdata);

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
            Attribute aggr_class_attr =
                c->get_attribute("class.aggregatable");

            Variant   usec_val  = Variant("usec");
            Variant   sec_val   = Variant("sec");
            Variant   true_val  = Variant(true);

            Variant   unit_val  = usec_val;

            std::string unitstr = config.get("unit").to_string();

            if (unitstr == "sec") {
                unit_val = sec_val;
                scale_factor = 1e-6;
            } else if (unitstr != "usec")
                Log(0).stream() << chn->name() << ": timestamp: Unknown unit " << unitstr
                                << std::endl;

            Attribute meta_attr[2] = { aggr_class_attr, unit_attr };
            Variant   meta_vals[2] = { true_val,        unit_val  };

            timestamp_attr =
                c->create_attribute("time.timestamp", CALI_TYPE_UINT,
                                    CALI_ATTR_ASVALUE       |
                                    CALI_ATTR_SCOPE_PROCESS |
                                    CALI_ATTR_SKIP_EVENTS,
                                    1, &unit_attr, &sec_val);
            offset_attr_id =
                c->create_attribute("time.offset",    CALI_TYPE_UINT,
                                    CALI_ATTR_ASVALUE       |
                                    CALI_ATTR_SCOPE_THREAD  |
                                    CALI_ATTR_SKIP_EVENTS,
                                    1, &unit_attr, &usec_val).id();
            snapshot_duration_attr_id =
                c->create_attribute("time.duration",  CALI_TYPE_DOUBLE,
                                    CALI_ATTR_ASVALUE       |
                                    CALI_ATTR_SCOPE_THREAD  |
                                    CALI_ATTR_SKIP_EVENTS,
                                    2, meta_attr, meta_vals).id();
            inclusive_duration_attr_id =
                c->create_attribute("time.inclusive.duration", CALI_TYPE_DOUBLE,
                                    CALI_ATTR_ASVALUE       |
                                    CALI_ATTR_SCOPE_THREAD  |
                                    CALI_ATTR_SKIP_EVENTS,
                                    2, meta_attr, meta_vals).id();
            timerinfo_attr = 
                c->create_attribute(std::string("timer.info.") + std::to_string(chn->id()), CALI_TYPE_PTR,
                                    CALI_ATTR_ASVALUE       |
                                    CALI_ATTR_SCOPE_THREAD  |
                                    CALI_ATTR_SKIP_EVENTS   |
                                    CALI_ATTR_HIDDEN);
        }

        ~Timestamp() {
            std::lock_guard<std::mutex>
                g(info_obj_mutex);

            for (TimerInfo* ti : info_obj_list)
                delete ti;
        }

public:

    static void timestamp_register(Caliper* c, Channel* chn) {
        Timestamp* instance = new Timestamp(c, chn);

        chn->events().post_init_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->post_init_cb(c, chn);
            });
        chn->events().create_thread_evt.connect(
            [instance](Caliper* c, Channel*){
                instance->acquire_timerinfo(c);
            });
        chn->events().snapshot.connect(
            [instance](Caliper* c, Channel* chn, int scopes, const SnapshotRecord* info, SnapshotRecord* snapshot){
                instance->snapshot_cb(c, chn, scopes, info, snapshot);
            });
        chn->events().finish_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->finish_cb(c, chn);
                delete instance;
            });

        Log(1).stream() << chn->name() << ": Registered timestamp service" << endl;
    }

}; // class Timestamp

const ConfigSet::Entry Timestamp::s_configdata[] = {
    { "snapshot_duration", CALI_TYPE_BOOL, "true",
      "Include duration of snapshot epoch with each context record",
      "Include duration of snapshot epoch with each context record"
    },
    { "offset", CALI_TYPE_BOOL, "false",
      "Include time offset (time since program start) with each context record",
      "Include time offset (time since program start) with each context record"
    },
    { "timestamp", CALI_TYPE_BOOL, "false",
      "Include absolute timestamp (POSIX time) with each context record",
      "Include absolute timestamp (POSIX time) with each context record"
    },
    { "inclusive_duration", CALI_TYPE_BOOL, "true",
      "Record inclusive duration of begin/end phases.",
      "Record inclusive duration of begin/end phases."
    },
    { "unit", CALI_TYPE_STRING, "sec",
      "Unit for time durations (sec or usec)",
      "Unit for time durations (sec or usec)"
    },
    ConfigSet::Terminator
};

}  // namespace


namespace cali
{

CaliperService timestamp_service = { "timestamp", ::Timestamp::timestamp_register };

} // namespace cali
