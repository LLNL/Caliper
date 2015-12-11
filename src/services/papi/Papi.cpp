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

///@file  Papi.cpp
///@brief PAPI provider for caliper records

#include "../CaliperService.h"

#include <Caliper.h>
#include <Snapshot.h>

#include <RuntimeConfig.h>
#include <ContextRecord.h>
#include <Log.h>

#include <util/split.hpp>

#include <pthread.h>

using namespace cali;
using namespace std;

#include <papi.h>

#define MAX_COUNTERS 16

namespace 
{

cali_id_t counter_attrbs[MAX_COUNTERS];

int       counter_events[MAX_COUNTERS];
long long counter_values[MAX_COUNTERS];

int       num_counters { 0 };


static const ConfigSet::Entry s_configdata[] = {
    { "counters", CALI_TYPE_STRING, "",
      "List of PAPI events to record",
      "List of PAPI events to record, separated by ':'" 
    },
    ConfigSet::Terminator
};

void push_counter(Caliper* c, int scope, const Entry*, Snapshot* sbuf) {
    if (num_counters < 1)
        return;

    if (PAPI_accum_counters(counter_values, num_counters) != PAPI_OK) {
        Log(1).stream() << "PAPI failed to accumulate counters!" << endl;
        return;
    }

    Snapshot::Sizes     sizes = sbuf->capacity();
    Snapshot::Addresses addr  = sbuf->addresses(); 

    int m = std::min(num_counters, sizes.n_data);

    for (int i = 0; i < m; ++i)
        addr.immediate_data[i] = Variant(static_cast<uint64_t>(counter_values[i]));

    copy_n(counter_attrbs, m, addr.immediate_attr);

    sizes.n_nodes = 0;
    sizes.n_data  = m;
    sizes.n_attr  = m;

    sbuf->commit(sizes);
}

void papi_init(Caliper* c) {
    if (PAPI_start_counters(counter_events, num_counters) != PAPI_OK)
        Log(1).stream() << "PAPI counters failed to initialize!" << endl;
    else
        Log(1).stream() << "PAPI counters initialized successfully" << endl;
}

void papi_finish(Caliper* c) {
    if (PAPI_stop_counters(counter_values, num_counters) != PAPI_OK)
        Log(1).stream() << "PAPI counters failed to stop!" << endl;
    else
        Log(1).stream() << "PAPI counters stopped successfully" << endl;
}

void setup_events(Caliper* c, const string& eventstring)
{
    vector<string> events;

    util::split(eventstring, ':', back_inserter(events));

    num_counters = 0;

    for (string& event : events) {
        int code;

        if (PAPI_event_name_to_code(const_cast<char*>(event.c_str()), &code) != PAPI_OK) {
            Log(0).stream() << "Unable to register PAPI counter \"" << event << '"' << endl;
            continue;
        }

        if (num_counters < MAX_COUNTERS) {
            Attribute attr =
                c->create_attribute(string("papi.")+event, CALI_TYPE_UINT,
                                    CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);

            counter_events[num_counters] = code;
            counter_attrbs[num_counters] = attr.id();

            ++num_counters;
        } else
            Log(0).stream() << "Maximum number of PAPI counters exceeded; dropping \"" 
                            << event << '"' << endl;
    }
}

// Initialization handler
void papi_register(Caliper* c) {
    int ret = PAPI_library_init(PAPI_VER_CURRENT);
    
    if (ret != PAPI_VER_CURRENT && ret > 0) {
        Log(0).stream() << "PAPI version mismatch: found " 
                        << ret << ", expected " << PAPI_VER_CURRENT << endl;
        return;
    }        

    // PAPI_thread_init(pthread_self);
    
    if (PAPI_is_initialized() == PAPI_NOT_INITED) {
        Log(0).stream() << "PAPI library is not initialized" << endl;
        return;
    }

    setup_events(c, RuntimeConfig::init("papi", s_configdata).get("counters").to_string());

    if (num_counters < 1) {
        Log(1).stream() << "No PAPI counters registered, dropping PAPI service" << endl;
        return;
    }

    // add callback for Caliper::get_context() event
    c->events().post_init_evt.connect(&papi_init);
    c->events().finish_evt.connect(&papi_finish);
    c->events().snapshot.connect(&push_counter);

    Log(1).stream() << "Registered PAPI service" << endl;
}

} // namespace


namespace cali 
{
    CaliperService PapiService = { "papi", ::papi_register };
} // namespace cali
