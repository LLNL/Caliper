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

typedef std::map<cali_id_t, Attribute> AttributeMap;

std::mutex               level_attributes_lock;
AttributeMap             level_attributes;

std::vector<std::string> trigger_attr_names;

Attribute                trigger_begin_attr { Attribute::invalid };
Attribute                trigger_end_attr   { Attribute::invalid };
Attribute                trigger_set_attr   { Attribute::invalid };

Attribute                trigger_level_attr { Attribute::invalid };

void create_attribute_cb(Caliper* c, const Attribute& attr)
{
    if (attr.skip_events())
        return;

    std::vector<std::string>::iterator it = find(trigger_attr_names.begin(), trigger_attr_names.end(), attr.name());

    if (it != trigger_attr_names.end() || trigger_attr_names.empty()) {
        std::string name = "cali.lvl.";
        name.append(std::to_string(attr.id()));

        Attribute lvl_attr = 
            c->create_attribute(name, CALI_TYPE_INT, 
                                CALI_ATTR_ASVALUE     | 
                                CALI_ATTR_HIDDEN      | 
                                CALI_ATTR_SKIP_EVENTS | 
                                (attr.properties() & CALI_ATTR_SCOPE_MASK));

        std::lock_guard<std::mutex>
            g(level_attributes_lock);
        
        level_attributes.insert(std::make_pair(attr.id(), lvl_attr));
    }
}

Attribute get_level_attribute(const Attribute& attr)
{
    std::lock_guard<std::mutex>
        g(level_attributes_lock);

    auto it = level_attributes.find(attr.id());

    return (it == level_attributes.end()) ? Attribute::invalid : it->second;
}

void event_begin_cb(Caliper* c, const Attribute& attr)
{
    Attribute lvl_attr(get_level_attribute(attr));

    if (lvl_attr == Attribute::invalid)
        return;

    if (enable_snapshot_info) {
        unsigned  lvl = 1;
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

        Attribute attrs[2] = { trigger_level_attr, trigger_begin_attr };
        Variant   vals[2]  = { v_lvl, Variant(attr.id()) };

        Entry trigger_info = c->make_entry(2, attrs, vals);

        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);
    } else {
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
    }
}

void event_set_cb(Caliper* c, const Attribute& attr)
{
    Attribute lvl_attr(get_level_attribute(attr));

    if (lvl_attr == Attribute::invalid)
        return;

    if (enable_snapshot_info) {
        unsigned  lvl(1);
        Variant v_lvl(lvl);

        // The level for set() is always 1
        // FIXME: ... except for set_path()??
        c->set(lvl_attr, v_lvl);

        // Construct the trigger info entry

        Attribute attrs[2] = { trigger_level_attr, trigger_set_attr };
        Variant   vals[2]  = { v_lvl, Variant(attr.id()) };

        Entry trigger_info = c->make_entry(2, attrs, vals);

        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);
    } else {
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
    }
}

void event_end_cb(Caliper* c, const Attribute& attr)
{
    Attribute lvl_attr(get_level_attribute(attr));

    if (lvl_attr == Attribute::invalid)
        return;

    if (enable_snapshot_info) {
        unsigned  lvl = 0;
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

        Attribute attrs[2] = { trigger_level_attr, trigger_end_attr };
        Variant   vals[2]  = { v_p_lvl, Variant(attr.id()) };

        Entry trigger_info = c->make_entry(2, attrs, vals);

        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);
    } else {
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
    }
}

void event_trigger_register(Caliper* c)
{
    level_attributes_lock.unlock();
    
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

    c->events().create_attr_evt.connect(&create_attribute_cb);

    c->events().pre_begin_evt.connect(&event_begin_cb);
    c->events().pre_set_evt.connect(&event_set_cb);
    c->events().pre_end_evt.connect(&event_end_cb);

    Log(1).stream() << "Registered event trigger service" << endl;
}

} // namespace

namespace cali
{
    CaliperService EventTriggerService { "event", &::event_trigger_register };
}
