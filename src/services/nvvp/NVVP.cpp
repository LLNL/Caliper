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

/// @file  NVVP.cpp
/// @brief Caliper NVVP service

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

#include "nvToolsExt.h"

        const uint32_t colors[] = { 0x0000ff00, 0x000000ff, 0x00ffff00, 0x00ff00ff, 0x0000ffff, 0x00ff0000, 0x00ffffff };
        const int num_colors = sizeof(colors)/sizeof(uint32_t);

#define CALIPER_NVVP_PUSH_RANGE(name,cid) { \
            int color_id = cid; \
            color_id = color_id%num_colors;\
            nvtxEventAttributes_t eventAttrib = {0}; \
            eventAttrib.version = NVTX_VERSION; \
            eventAttrib.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE; \
            eventAttrib.colorType = NVTX_COLOR_ARGB; \
            eventAttrib.color = colors[color_id]; \
            eventAttrib.messageType = NVTX_MESSAGE_TYPE_ASCII; \
            eventAttrib.message.ascii = name; \
            nvtxRangePushEx(&eventAttrib); \
}
#define CALIPER_NVVP_POP_RANGE nvtxRangePop();

const ConfigSet::Entry   configdata[] = {
    { "trigger", CALI_TYPE_STRING, "phase",
      "Attribute on which to trigger NVVP regions",
    },
    ConfigSet::Terminator
};

ConfigSet                config;
cali_id_t                trigger_event_id;
std::string              trigger_event_name;

static inline bool attributeNameInteresting(const Attribute& attr){
    return(attr.name()==trigger_event_name);
} 
static inline bool isTriggerAttribute(const Attribute& attr){
    return(attr.id()==trigger_event_id);
}
void create_attribute_cb(Caliper* c, const Attribute& attr)
{
    if(attributeNameInteresting(attr)){
        trigger_event_id = attr.id();
    }
}

void nvvp_begin_cb(Caliper* c, const Attribute& attr, const Variant& value)
{
    if(isTriggerAttribute(attr)){
        CALIPER_NVVP_PUSH_RANGE(value.to_string().c_str(),1)
    }
}

void nvvp_end_cb(Caliper* c, const Attribute& attr, const Variant& value)
{
    if(isTriggerAttribute(attr)){
        CALIPER_NVVP_POP_RANGE
    }
}

void nvvp_trigger_register(Caliper* c)
{
    config = RuntimeConfig::init("event", configdata);
    trigger_event_name = config.get("trigger").to_string();

    c->events().pre_begin_evt.connect(&nvvp_begin_cb);
    c->events().pre_end_evt.connect(&nvvp_end_cb);

    Log(1).stream() << "Registered event trigger service" << endl;
}

} // namespace

namespace cali
{
    CaliperService NVVPTriggerService { "nvvp", &::nvvp_trigger_register };
}
