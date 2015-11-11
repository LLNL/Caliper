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
#include <SigsafeRWLock.h>

#include <Log.h>
#include <RuntimeConfig.h>

#include <util/split.hpp>

#include <algorithm>
#include <iterator>
#include <set>
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

    ConfigSet::Terminator
};

ConfigSet                config;

std::set<cali_id_t>      trigger_attr_ids;
std::vector<std::string> trigger_attr_names;

SigsafeRWLock            trigger_list_lock;


void create_attribute_cb(Caliper* c, const Attribute& attr)
{
    std::vector<std::string>::iterator it = find(trigger_attr_names.begin(), trigger_attr_names.end(), attr.name());

    trigger_list_lock.wlock();

    if (it != trigger_attr_names.end())
        trigger_attr_ids.insert(attr.id());

    trigger_list_lock.unlock();
}

void event_cb(Caliper* c, const Attribute& attr)
{
    if (!trigger_attr_names.empty()) {
        bool trigger = false;

        trigger_list_lock.rlock();
        trigger = trigger_attr_ids.count(attr.id()) > 0;
        trigger_list_lock.unlock();

        if (!trigger)
            return;
    }

    c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS);
}

void event_trigger_register(Caliper* c) {
    // parse the configuration & set up triggers

    config = RuntimeConfig::init("event", configdata);

    util::split(config.get("trigger").to_string(), ':', 
                std::back_inserter(trigger_attr_names));

    // register callbacks

    c->events().create_attr_evt.connect(&create_attribute_cb);

    c->events().pre_begin_evt.connect(&event_cb);
    c->events().pre_set_evt.connect(&event_cb);
    c->events().pre_end_evt.connect(&event_cb);

    Log(1).stream() << "Registered event trigger service" << endl;
}

} // namespace

namespace cali
{
    CaliperService EventTriggerService { "event", &::event_trigger_register };
}
