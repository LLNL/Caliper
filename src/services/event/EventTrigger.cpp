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

/// @file  EventTrigger.cpp
/// @brief Caliper event trigger

#include "../CaliperService.h"

#include <Caliper.h>
#include <EntryList.h>

#include <Log.h>
#include <RuntimeConfig.h>

#include <util/split.hpp>

#include <algorithm>
#include <iterator>
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

typedef std::map<cali_id_t, EventAttributes> AttributeMap;

std::mutex               event_attributes_lock;
AttributeMap             event_attributes_map;

std::vector<std::string> trigger_attr_names;

Attribute                trigger_begin_attr { Attribute::invalid };
Attribute                trigger_end_attr   { Attribute::invalid };
Attribute                trigger_set_attr   { Attribute::invalid };

Attribute                trigger_level_attr { Attribute::invalid };

    
void pre_create_attribute_cb(Caliper* c, const std::string& name, cali_attr_type* type, int* prop)
{
    if (trigger_attr_names.empty() || *prop & CALI_ATTR_SKIP_EVENTS)
        return;

    // add SKIP_EVENTS property to all non-trigger attributes
    
    std::vector<std::string>::iterator it =
        std::find(trigger_attr_names.begin(), trigger_attr_names.end(), name);
    
    if (it == trigger_attr_names.end())
        *prop |= CALI_ATTR_SKIP_EVENTS;
}

void create_attribute_cb(Caliper* c, const Attribute& attr)
{
    if (attr.skip_events())
        return;

    EventAttributes event_attributes;

    struct evt_attr_setup_t {
        std::string prefix;
        Attribute*  attr_ptr;
    } evt_attr_setup[] = {
        { "event.begin.", &(event_attributes.begin_attr) },
        { "event.set.",   &(event_attributes.set_attr)   },
        { "event.end.",   &(event_attributes.end_attr)   }
    };

    for ( evt_attr_setup_t setup : evt_attr_setup ) {
        std::string name = setup.prefix;
        name.append(attr.name());

        *(setup.attr_ptr) =
            c->create_attribute(name, attr.type(), attr.properties() | CALI_ATTR_SKIP_EVENTS);
    }
        
    std::string name = "cali.lvl.";
    name.append(std::to_string(attr.id()));

    event_attributes.lvl_attr = 
        c->create_attribute(name, CALI_TYPE_INT, 
                            CALI_ATTR_ASVALUE     | 
                            CALI_ATTR_HIDDEN      | 
                            CALI_ATTR_SKIP_EVENTS | 
                            (attr.properties() & CALI_ATTR_SCOPE_MASK));

    std::lock_guard<std::mutex>
        g(event_attributes_lock);
        
    event_attributes_map.insert(std::make_pair(attr.id(), event_attributes));
}

bool get_event_attributes(const Attribute& attr, EventAttributes& evt_attr)
{
    std::lock_guard<std::mutex>
        g(event_attributes_lock);

    auto it = event_attributes_map.find(attr.id());

    if (it != event_attributes_map.end()) {
        evt_attr = it->second;
        return true;
    }

    return false;
}

void event_begin_cb(Caliper* c, const Attribute& attr, const Variant& value)
{
    EventAttributes event_attr;

    if (!get_event_attributes(attr, event_attr))
        return;

    if (enable_snapshot_info) {
        unsigned  lvl = 1;
        Variant v_lvl(lvl), v_p_lvl;

        // Use Caliper::exchange() to accelerate common-case of setting new hierarchy level to 1.
        // If previous level was > 0, we need to increment it further

        // FIXME: There may be a race condition between c->exchange() and c->set()
        // when two threads update a process-scope attribute.
        // Can fix that with a more general c->update(update_fn) function

        v_p_lvl = c->exchange(event_attr.lvl_attr, v_lvl);
        lvl     = v_p_lvl.to_uint();

        if (lvl > 0) {
            v_lvl = Variant(++lvl);
            c->set(event_attr.lvl_attr, v_lvl);
        }

        // Construct the trigger info entry

        Attribute attrs[3] = { trigger_level_attr, trigger_begin_attr, event_attr.begin_attr };
        Variant   vals[3]  = { v_lvl, Variant(attr.id()), value };

        EntryList::FixedEntryList<3> trigger_info_data;
        EntryList trigger_info(trigger_info_data);

        c->make_entrylist(3, attrs, vals, trigger_info);
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);
    } else {
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
    }
}

void event_set_cb(Caliper* c, const Attribute& attr, const Variant& value)
{
    EventAttributes event_attr;

    if (!get_event_attributes(attr, event_attr))
        return;

    if (enable_snapshot_info) {
        unsigned  lvl(1);
        Variant v_lvl(lvl);

        // The level for set() is always 1
        // FIXME: ... except for set_path()??
        c->set(event_attr.lvl_attr, v_lvl);

        // Construct the trigger info entry

        Attribute attrs[3] = { trigger_level_attr, trigger_set_attr, event_attr.set_attr };
        Variant   vals[3]  = { v_lvl, Variant(attr.id()), value };

        EntryList::FixedEntryList<3> trigger_info_data;
        EntryList trigger_info(trigger_info_data);

        c->make_entrylist(3, attrs, vals, trigger_info);
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);
    } else {
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
    }
}

void event_end_cb(Caliper* c, const Attribute& attr, const Variant& value)
{
    EventAttributes event_attr;

    if (!get_event_attributes(attr, event_attr))
        return;

    if (enable_snapshot_info) {
        unsigned  lvl = 0;
        Variant v_lvl(lvl), v_p_lvl;

        // Use Caliper::exchange() to accelerate common-case of setting new level to 0.
        // If previous level was > 1, we need to update it again

        v_p_lvl = c->exchange(event_attr.lvl_attr, v_lvl);

        if (v_p_lvl.empty())
            return;
        
        lvl     = v_p_lvl.to_uint();

        if (lvl > 1)
            c->set(event_attr.lvl_attr, Variant(--lvl));

        // Construct the trigger info entry with previous level

        Attribute attrs[3] = { trigger_level_attr, trigger_end_attr, event_attr.end_attr };
        Variant   vals[3]  = { v_p_lvl, Variant(attr.id()), value };

        EntryList::FixedEntryList<3> trigger_info_data;
        EntryList trigger_info(trigger_info_data);

        c->make_entrylist(3, attrs, vals, trigger_info);
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);
    } else {
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
    }
}

void event_trigger_register(Caliper* c)
{
    event_attributes_lock.unlock();
    
    // parse the configuration & set up triggers

    config = RuntimeConfig::init("event", configdata);

    util::split(config.get("trigger").to_string(), ':', 
                std::back_inserter(trigger_attr_names));

    enable_snapshot_info = config.get("enable_snapshot_info").to_bool();

    // register trigger events

    if (enable_snapshot_info) {
        trigger_begin_attr = 
            c->create_attribute("cali.snapshot.event.begin", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        trigger_set_attr = 
            c->create_attribute("cali.snapshot.event.set",   CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        trigger_end_attr = 
            c->create_attribute("cali.snapshot.event.end",   CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        trigger_level_attr = 
            c->create_attribute("cali.snapshot.event.attr.level", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
    }

    // register callbacks

    c->events().pre_create_attr_evt.connect(&pre_create_attribute_cb);
    c->events().create_attr_evt.connect(&create_attribute_cb);

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
