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
    chrono::time_point<chrono::high_resolution_clock> tstart;

    Attribute timestamp_attr { Attribute::invalid } ;
    Attribute timeoffs_attr  { Attribute::invalid } ;
    cali_id_t snapshot_duration_attr_id { CALI_INV_ID };
    cali_id_t phase_duration_attr_id    { CALI_INV_ID };

    bool      record_timestamp;
    bool      record_offset;
    bool      record_duration;
    bool      record_phases;

    double    scale_factor   { 1.0 };

    Attribute begin_evt_attr { Attribute::invalid };
    Attribute set_evt_attr   { Attribute::invalid };
    Attribute end_evt_attr   { Attribute::invalid };
    Attribute lvl_attr       { Attribute::invalid };

    typedef std::map<uint64_t, Attribute> OffsetAttributeMap;

    std::mutex         offset_attributes_mutex;
    OffsetAttributeMap offset_attributes;

    static const ConfigSet::Entry s_configdata[];

    uint64_t make_offset_attribute_key(cali_id_t attr_id, unsigned level) {
        // assert((level   & 0xFFFF)         == 0);
        // assert((attr_id & 0xFFFFFFFFFFFF) == 0);

        uint64_t lvl_k = static_cast<uint64_t>(level) << 48;

        return lvl_k | attr_id;
    }

    // Get or create a hidden offset attribute for the given attribute and its
    // current hierarchy level

    Attribute make_offset_attribute(Caliper* c, cali_id_t attr_id, unsigned level) {
        uint64_t key = make_offset_attribute_key(attr_id, level);

        {
            std::lock_guard<std::mutex>  lock(offset_attributes_mutex);
            OffsetAttributeMap::iterator it = offset_attributes.find(key);

            if (it != offset_attributes.end())
                return it->second;
        }

        // Not found -> create new entry

        std::string name = "offs.";
        name.append(std::to_string(attr_id));
        name.append(".");
        name.append(std::to_string(level));

        Attribute offs_attr =
            c->create_attribute(name, CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE     |
                                CALI_ATTR_SKIP_EVENTS |
                                CALI_ATTR_HIDDEN      |
                                CALI_ATTR_SCOPE_THREAD); // FIXME: needs to be original attribute's scope

        std::lock_guard<std::mutex> lock(offset_attributes_mutex);
        offset_attributes.insert(std::make_pair(key, offs_attr));

        return offs_attr;
    }

    // Get existing hidden offset attribute for the given attribute and its
    // current hierarchy level

    Attribute find_offset_attribute(Caliper* c, cali_id_t attr_id, unsigned level) {
        uint64_t key = make_offset_attribute_key(attr_id, level);

        {
            std::lock_guard<std::mutex>  lock(offset_attributes_mutex);
            OffsetAttributeMap::iterator it = offset_attributes.find(key);

            if (it != offset_attributes.end())
                return it->second;
        }

        return Attribute::invalid;
    }

    void snapshot_cb(Caliper* c, Channel* chn, int scope, const SnapshotRecord* info, SnapshotRecord* sbuf) {
        auto now = chrono::high_resolution_clock::now();

        if ((record_duration || record_phases || record_offset) && scope & CALI_SCOPE_THREAD) {
            uint64_t  usec = chrono::duration_cast<std::chrono::microseconds>(now - tstart).count();
            Variant v_usec(usec);
            Variant v_offs(c->exchange(chn, timeoffs_attr, v_usec));

            if (record_duration && !v_offs.empty())
                sbuf->append(snapshot_duration_attr_id, Variant(scale_factor * (usec - v_offs.to_uint())));

            if (record_phases && info) {
                Entry event = info->get(begin_evt_attr);

                cali_id_t evt_attr_id;
                Variant   v_level;

                if (event.is_empty())
                    event = info->get(set_evt_attr);
                if (event.is_empty())
                    event = info->get(end_evt_attr);
                if (event.is_empty())
                    goto record_phases_exit;

                evt_attr_id = event.value().to_id();
                v_level     = info->get(lvl_attr).value();

                if (evt_attr_id == CALI_INV_ID || v_level.empty())
                    goto record_phases_exit;

                if (event.attribute() == begin_evt_attr.id()) {
                    // begin/set event: save time for current entry

                    c->set(chn, make_offset_attribute(c, evt_attr_id, v_level.to_uint()), v_usec);
                } else if (event.attribute() == set_evt_attr.id())   {
                    // set event: get saved time for current entry and calculate duration

                    Variant v_p_usec    =
                        c->exchange(chn, make_offset_attribute(c, evt_attr_id, v_level.to_uint()), v_usec);

                    if (!v_p_usec.empty())
                        sbuf->append(phase_duration_attr_id, Variant(scale_factor * (usec - v_p_usec.to_uint())));
                } else if (event.attribute() == end_evt_attr.id())   {
                    // end event: get saved time for current entry and calculate duration

                    Attribute offs_attr =
                        find_offset_attribute(c, evt_attr_id, v_level.to_uint());

                    if (offs_attr == Attribute::invalid)
                        goto record_phases_exit;

                    Variant v_p_usec    =
                        c->exchange(chn, offs_attr, Variant());

                    if (!v_p_usec.empty())
                        sbuf->append(phase_duration_attr_id, Variant(scale_factor * (usec - v_p_usec.to_uint())));
                }
            record_phases_exit:
                ;
            }
        }

        if (record_timestamp && (scope & CALI_SCOPE_PROCESS))
            sbuf->append(timestamp_attr.id(),
                         Variant(cali_make_variant_from_uint(
                                 chrono::duration_cast<chrono::nanoseconds>(chrono::system_clock::now().time_since_epoch()).count())));
    }

    void post_init_cb(Caliper* c, Channel* chn) {
        // Find begin/end event snapshot event info attributes

        begin_evt_attr = c->get_attribute("cali.event.begin");
        set_evt_attr   = c->get_attribute("cali.event.set");
        end_evt_attr   = c->get_attribute("cali.event.end");
        lvl_attr       = c->get_attribute("cali.event.attr.level");

        if (begin_evt_attr == Attribute::invalid ||
            set_evt_attr   == Attribute::invalid ||
            end_evt_attr   == Attribute::invalid ||
            lvl_attr       == Attribute::invalid) {
            if (record_phases)
                Log(1).stream() << chn->name() << ": Timestamp: Note: event trigger attributes not registered,\n"
                    "    disabling phase timers." << std::endl;

            record_phases = false;
        }
    }

    Timestamp(Caliper* c, Channel* chn)
        : tstart(chrono::high_resolution_clock::now())
        {
            ConfigSet config = chn->config().init("timer", s_configdata);

            record_duration  = config.get("snapshot_duration").to_bool();
            record_offset    = config.get("offset").to_bool();
            record_timestamp = config.get("timestamp").to_bool();
            record_phases    = config.get("inclusive_duration").to_bool();

            Attribute unit_attr =
                c->create_attribute("time.unit", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
            Attribute aggr_class_attr =
                c->get_attribute("class.aggregatable");

            Variant   usec_val  = Variant(CALI_TYPE_STRING, "usec", 5);
            Variant   sec_val   = Variant(CALI_TYPE_STRING, "sec",  4);
            Variant   true_val  = Variant(true);

            int hide_offset =
                ((record_duration || record_phases) && !record_offset ? CALI_ATTR_HIDDEN : 0);

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
            timeoffs_attr =
                c->create_attribute("time.offset",    CALI_TYPE_UINT,
                                    CALI_ATTR_ASVALUE       |
                                    CALI_ATTR_SCOPE_THREAD  |
                                    CALI_ATTR_SKIP_EVENTS | hide_offset,
                                    1, &unit_attr, &usec_val);
            snapshot_duration_attr_id =
                c->create_attribute("time.duration",  CALI_TYPE_DOUBLE,
                                    CALI_ATTR_ASVALUE       |
                                    CALI_ATTR_SCOPE_THREAD  |
                                    CALI_ATTR_SKIP_EVENTS,
                                    2, meta_attr, meta_vals).id();
            phase_duration_attr_id =
                c->create_attribute("time.inclusive.duration", CALI_TYPE_DOUBLE,
                                    CALI_ATTR_ASVALUE       |
                                    CALI_ATTR_SCOPE_THREAD  |
                                    CALI_ATTR_SKIP_EVENTS,
                                    2, meta_attr, meta_vals).id();

            c->set(chn, timeoffs_attr, Variant(cali_make_variant_from_uint(0)));
        }

public:

    static void timestamp_register(Caliper* c, Channel* chn) {
        Timestamp* instance = new Timestamp(c, chn);

        chn->events().post_init_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->post_init_cb(c, chn);
            });
        chn->events().snapshot.connect(
            [instance](Caliper* c, Channel* chn, int scopes, const SnapshotRecord* info, SnapshotRecord* snapshot){
                instance->snapshot_cb(c, chn, scopes, info, snapshot);
            });
        chn->events().finish_evt.connect(
            [instance](Caliper* c, Channel* chn){
                delete instance;
            });

        Log(1).stream() << chn->name() << ": Registered timestamp service" << endl;
    }

}; // class Timestamp

const ConfigSet::Entry Timestamp::s_configdata[] = {
    { "snapshot_duration", CALI_TYPE_BOOL, "false",
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
    { "unit", CALI_TYPE_STRING, "usec",
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
