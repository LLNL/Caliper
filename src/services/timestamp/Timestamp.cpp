// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Timestamp.cpp
// Timestamp provider for caliper records

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/ContextRecord.h"
#include "caliper/common/Log.h"

#include <cassert>
#include <chrono>
#include <map>
#include <mutex>
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
    Attribute snapshot_duration_attr { Attribute::invalid };
    Attribute phase_duration_attr    { Attribute::invalid };

    bool      record_timestamp;
    bool      record_offset;
    bool      record_duration;
    bool      record_phases;

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

    void snapshot_cb(Caliper* c, Experiment* exp, int scope, const SnapshotRecord* info, SnapshotRecord* sbuf) {
        auto now = chrono::high_resolution_clock::now();

        if ((record_duration || record_phases || record_offset) && scope & CALI_SCOPE_THREAD) {
            uint64_t  usec = chrono::duration_cast<chrono::microseconds>(now - tstart).count();
            Variant v_usec = Variant(usec);
            Variant v_offs = c->exchange(exp, timeoffs_attr, v_usec);

            if (record_duration && !v_offs.empty()) {
                uint64_t duration = usec - v_offs.to_uint();

                sbuf->append(snapshot_duration_attr.id(), Variant(duration));
            }

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

                    c->set(exp, make_offset_attribute(c, evt_attr_id, v_level.to_uint()), v_usec);
                } else if (event.attribute() == set_evt_attr.id())   {
                    // set event: get saved time for current entry and calculate duration

                    Variant v_p_usec    = 
                        c->exchange(exp, make_offset_attribute(c, evt_attr_id, v_level.to_uint()), v_usec);

                    if (!v_p_usec.empty())
                        sbuf->append(phase_duration_attr.id(), Variant(usec - v_p_usec.to_uint()));
                } else if (event.attribute() == end_evt_attr.id())   {
                    // end event: get saved time for current entry and calculate duration

                    Attribute offs_attr = 
                        find_offset_attribute(c, evt_attr_id, v_level.to_uint());

                    if (offs_attr == Attribute::invalid)
                        goto record_phases_exit;

                    Variant v_p_usec    = 
                        c->exchange(exp, offs_attr, Variant());

                    if (!v_p_usec.empty())
                        sbuf->append(phase_duration_attr.id(), Variant(usec - v_p_usec.to_uint()));
                }
            record_phases_exit:
                ;
            }
        }

        if (record_timestamp && (scope & CALI_SCOPE_PROCESS))
            sbuf->append(timestamp_attr.id(),
                         Variant(cali_make_variant_from_uint(chrono::system_clock::to_time_t(chrono::system_clock::now()))));
    }

    void post_init_cb(Caliper* c, Experiment* exp) {
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
                Log(1).stream() << exp->name() << ": Timestamp: Note: event trigger attributes not registered,\n"
                    "    disabling phase timers." << std::endl;

            record_phases = false;
        }
    }

    Timestamp(Caliper* c, Experiment* exp)
        : tstart(chrono::high_resolution_clock::now())
        {
            ConfigSet config = exp->config().init("timer", s_configdata);

            record_duration  = config.get("snapshot_duration").to_bool();
            record_offset    = config.get("offset").to_bool();
            record_timestamp = config.get("timestamp").to_bool();
            record_phases    = config.get("inclusive_duration").to_bool();

            Attribute unit_attr = 
                c->create_attribute("time.unit", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
            Attribute aggr_class_attr = 
                c->get_attribute("class.aggregatable");

            Variant   usec_val  = Variant(CALI_TYPE_STRING, "usec", 4);
            Variant   sec_val   = Variant(CALI_TYPE_STRING, "sec",  3);
            Variant   true_val  = Variant(true);
    
            int hide_offset =
                ((record_duration || record_phases) && !record_offset ? CALI_ATTR_HIDDEN : 0);

            Attribute meta_attr[2] = { aggr_class_attr, unit_attr };
            Variant   meta_vals[2] = { true_val,        usec_val  };

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
            snapshot_duration_attr = 
                c->create_attribute("time.duration",  CALI_TYPE_UINT, 
                                    CALI_ATTR_ASVALUE       | 
                                    CALI_ATTR_SCOPE_THREAD  | 
                                    CALI_ATTR_SKIP_EVENTS,
                                    2, meta_attr, meta_vals);
            phase_duration_attr = 
                c->create_attribute("time.inclusive.duration", CALI_TYPE_UINT, 
                                    CALI_ATTR_ASVALUE       |
                                    CALI_ATTR_SCOPE_THREAD  | 
                                    CALI_ATTR_SKIP_EVENTS,
                                    2, meta_attr, meta_vals);

            c->set(exp, timeoffs_attr, Variant(static_cast<uint64_t>(0)));
        }

public:

    static void timestamp_register(Caliper* c, Experiment* exp) {
        Timestamp* instance = new Timestamp(c, exp);
        
        exp->events().post_init_evt.connect(
            [instance](Caliper* c, Experiment* exp){
                instance->post_init_cb(c, exp);
            });
        exp->events().snapshot.connect(
            [instance](Caliper* c, Experiment* exp, int scopes, const SnapshotRecord* info, SnapshotRecord* snapshot){
                instance->snapshot_cb(c, exp, scopes, info, snapshot);
            });
        exp->events().finish_evt.connect(
            [instance](Caliper* c, Experiment* exp){
                delete instance;
            });

        Log(1).stream() << exp->name() << ": Registered timestamp service" << endl;
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
    ConfigSet::Terminator
};

}  // namespace


namespace cali
{

CaliperService timestamp_service = { "timestamp", ::Timestamp::timestamp_register };

} // namespace cali
