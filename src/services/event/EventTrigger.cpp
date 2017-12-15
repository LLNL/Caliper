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

// EventTrigger.cpp
// Caliper event trigger

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <mutex>
#include <string>
#include <vector>

using namespace cali;
using namespace std;


namespace
{

const ConfigSet::Entry   configdata[] = {
    { "trigger", CALI_TYPE_STRING, "",
      "List of attributes that trigger measurement snapshots.",
      "Colon-separated list of attributes that trigger measurement snapshots.\n"
      "If empty, all user attributes trigger measurement snapshots."
    },
    { "enable_snapshot_info", CALI_TYPE_BOOL, "true",
      "Enable snapshot info records",
      "Enable snapshot info records."
    },

    ConfigSet::Terminator
};

ConfigSet                config;

bool                     enable_snapshot_info;

struct EventAttributes {
    Attribute begin_attr;
    Attribute set_attr;
    Attribute end_attr;
    Attribute lvl_attr;
};

std::vector<std::string> trigger_attr_names;

Attribute                trigger_begin_attr { Attribute::invalid };
Attribute                trigger_end_attr   { Attribute::invalid };
Attribute                trigger_set_attr   { Attribute::invalid };

Attribute                trigger_level_attr { Attribute::invalid };

Attribute                event_info_attr    { Attribute::invalid };

EventAttributes
make_event_attributes(Caliper* c, const std::string& name, cali_attr_type type, int prop)
{
    EventAttributes event_attributes;

    struct evt_attr_setup_t {
        std::string prefix;
        Attribute*  attr_ptr;
    } evt_attr_setup[] = {
        { "event.begin#", &(event_attributes.begin_attr) },
        { "event.set#",   &(event_attributes.set_attr)   },
        { "event.end#",   &(event_attributes.end_attr)   }
    };

    for ( evt_attr_setup_t setup : evt_attr_setup ) {
        std::string s = setup.prefix;
        s.append(name);

        *(setup.attr_ptr) =
            c->create_attribute(s, type, (prop & ~CALI_ATTR_NESTED) | CALI_ATTR_SKIP_EVENTS);
    }

    std::string s = "cali.lvl#";
    s.append(name);

    event_attributes.lvl_attr =
        c->create_attribute(s, CALI_TYPE_INT,
                            CALI_ATTR_ASVALUE     |
                            CALI_ATTR_HIDDEN      |
                            CALI_ATTR_SKIP_EVENTS |
                            (prop & CALI_ATTR_SCOPE_MASK));

    return event_attributes;
}

void
pre_create_attribute_cb(Caliper* c, const std::string& name, cali_attr_type type, int* prop,  Node** node)
{
    if (*prop & CALI_ATTR_SKIP_EVENTS)
        return;

    std::vector<std::string>::iterator it =
        std::find(trigger_attr_names.begin(), trigger_attr_names.end(), name);

    if (trigger_attr_names.size() > 0 && it == trigger_attr_names.end()) {
        // Add SKIP_EVENTS property to all non-trigger attributes
        *prop |= CALI_ATTR_SKIP_EVENTS;
    } else if (enable_snapshot_info) {
        // Make and append derived event attributes
        EventAttributes evt_attr   = make_event_attributes(c, name, type, *prop);
        cali_id_t       evt_ids[4] = { evt_attr.begin_attr.id(),
                                       evt_attr.set_attr.id(),
                                       evt_attr.end_attr.id(),
                                       evt_attr.lvl_attr.id() };

        Variant v_events(CALI_TYPE_USR, evt_ids, sizeof(evt_ids));

        *node = c->make_tree_entry(event_info_attr, v_events, *node);

        assert(*node);
    }
}

void event_begin_cb(Caliper* c, const Attribute& attr, const Variant& value)
{
    if (enable_snapshot_info) {
        Variant v_ids = attr.get(event_info_attr);

        assert(!v_ids.empty());

        const cali_id_t* evt_info_attr_ids =
            static_cast<const cali_id_t*>(v_ids.data());

        assert(evt_info_attr_ids != nullptr);

        Attribute begin_attr = c->get_attribute(evt_info_attr_ids[0]);
        Attribute lvl_attr   = c->get_attribute(evt_info_attr_ids[3]);

        assert(begin_attr != Attribute::invalid);
        assert(lvl_attr   != Attribute::invalid);

        uint64_t  lvl = 1;
        Variant v_lvl(lvl), v_p_lvl;

        // Use Caliper::exchange() to accelerate common-case of setting new hierarchy level to 1.
        // If previous level was > 0, we need to increment it further

        // FIXME: There may be a race condition between c->exchange() and c->set()
        // when two threads update a process-scope attribute.
        // Can fix that with a more general c->update(update_fn) function

        v_p_lvl = c->exchange(lvl_attr, v_lvl);
        lvl     = v_p_lvl.to_uint();

        if (lvl > 0) {
            v_lvl = Variant(++lvl);
            c->set(lvl_attr, v_lvl);
        }

        // Construct the trigger info entry

        Attribute attrs[3] = { trigger_level_attr, trigger_begin_attr, begin_attr };
        Variant    vals[3] = { v_lvl, Variant(attr.id()), value };

        SnapshotRecord::FixedSnapshotRecord<3> trigger_info_data;
        SnapshotRecord trigger_info(trigger_info_data);

        c->make_entrylist(3, attrs, vals, trigger_info);
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);
    } else {
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
    }
}

void event_set_cb(Caliper* c, const Attribute& attr, const Variant& value)
{
    if (enable_snapshot_info) {
        Variant v_ids = attr.get(event_info_attr);

        assert(!v_ids.empty());

        const cali_id_t* evt_info_attr_ids =
            static_cast<const cali_id_t*>(v_ids.data());

        assert(evt_info_attr_ids != nullptr);

        Attribute set_attr = c->get_attribute(evt_info_attr_ids[1]);
        Attribute lvl_attr = c->get_attribute(evt_info_attr_ids[3]);

        uint64_t  lvl(1);
        Variant v_lvl(lvl);

        // The level for set() is always 1
        // FIXME: ... except for set_path()??
        c->set(lvl_attr, v_lvl);

        // Construct the trigger info entry

        Attribute attrs[3] = { trigger_level_attr, trigger_set_attr, set_attr };
        Variant    vals[3] = { v_lvl, Variant(attr.id()), value };

        SnapshotRecord::FixedSnapshotRecord<3> trigger_info_data;
        SnapshotRecord trigger_info(trigger_info_data);

        c->make_entrylist(3, attrs, vals, trigger_info);
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);
    } else {
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
    }
}

void event_end_cb(Caliper* c, const Attribute& attr, const Variant& value)
{
    if (enable_snapshot_info) {
        Variant v_ids = attr.get(event_info_attr);

        assert(!v_ids.empty());

        const cali_id_t* evt_info_attr_ids =
            static_cast<const cali_id_t*>(v_ids.data());

        assert(evt_info_attr_ids != nullptr);

        Attribute end_attr = c->get_attribute(evt_info_attr_ids[2]);
        Attribute lvl_attr = c->get_attribute(evt_info_attr_ids[3]);

        uint64_t  lvl = 0;
        Variant v_lvl(lvl), v_p_lvl;

        // Use Caliper::exchange() to accelerate common-case of setting new level to 0.
        // If previous level was > 1, we need to update it again

        v_p_lvl = c->exchange(lvl_attr, v_lvl);

        if (v_p_lvl.empty())
            return;

        lvl     = v_p_lvl.to_uint();

        if (lvl > 1)
            c->set(lvl_attr, Variant(--lvl));

        // Construct the trigger info entry with previous level

        Attribute attrs[3] = { trigger_level_attr, trigger_end_attr, end_attr };
        Variant    vals[3] = { v_p_lvl, Variant(attr.id()), value };

        SnapshotRecord::FixedSnapshotRecord<3> trigger_info_data;
        SnapshotRecord trigger_info(trigger_info_data);

        c->make_entrylist(3, attrs, vals, trigger_info);
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);
    } else {
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
    }
}

void event_trigger_register(Caliper* c)
{
    // parse the configuration & set up triggers

    config = RuntimeConfig::init("event", configdata);

    trigger_attr_names   = config.get("trigger").to_stringlist(",:");
    enable_snapshot_info = config.get("enable_snapshot_info").to_bool();

    // register trigger events

    if (enable_snapshot_info) {
        trigger_begin_attr =
            c->create_attribute("cali.event.begin",
                                CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        trigger_set_attr =
            c->create_attribute("cali.event.set",
                                CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        trigger_end_attr =
            c->create_attribute("cali.event.end",
                                CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        trigger_level_attr =
            c->create_attribute("cali.event.attr.level",
                                CALI_TYPE_UINT,
                                CALI_ATTR_SKIP_EVENTS |
                                CALI_ATTR_HIDDEN);
        event_info_attr =
            c->create_attribute("cali.event.attr.ids",
                                CALI_TYPE_USR,
                                CALI_ATTR_SKIP_EVENTS |
                                CALI_ATTR_HIDDEN);
    }

    // register callbacks

    c->events().pre_create_attr_evt.connect(&pre_create_attribute_cb);

    c->events().pre_begin_evt.connect(&event_begin_cb);
    c->events().pre_set_evt.connect(&event_set_cb);
    c->events().pre_end_evt.connect(&event_end_cb);

    Log(1).stream() << "Registered event trigger service" << endl;
}

} // namespace

namespace cali
{
    CaliperService event_service { "event", &::event_trigger_register };
}
