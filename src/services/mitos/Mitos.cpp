// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by Alfredo Gimenez, gimenez1@llnl.gov
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

///@file  Mitos.cpp
///@brief Mitos provider for caliper records

#include "../CaliperService.h"

#include <Caliper.h>
#include <Snapshot.h>

#include <RuntimeConfig.h>
#include <ContextRecord.h>
#include <Log.h>

#include <Mitos.h>

using namespace cali;
using namespace std;

namespace 
{

Attribute address_attr        { Attribute::invalid } ;
Attribute latency_attr        { Attribute::invalid } ;
Attribute timestamp_attr      { Attribute::invalid } ;
Attribute ip_attr             { Attribute::invalid } ;
Attribute datasource_attr     { Attribute::invalid } ;
Attribute cpu_attr            { Attribute::invalid } ;

ConfigSet config;

bool      record_address { true  };
bool      fresh_sample   { false };

perf_event_sample static_sample;

static const ConfigSet::Entry s_configdata[] = {
    { "mitos", CALI_TYPE_BOOL, "false",
      "Include memory access samples",
      "Include memory access samples"
    },
    ConfigSet::Terminator
};

static void sample_handler(perf_event_sample *sample, void *args) {
    // Copy to global static sample
    memcpy(&static_sample,sample,sizeof(perf_event_sample));

    fresh_sample = true;

    // Cush context to invoke push_sample
    Caliper *c = Caliper::instance();

    if (c == NULL) {
        std::cerr << "Null Caliper instance!\n";
    } else {
        c->push_snapshot(CALI_SCOPE_THREAD);
    }
}

void push_sample(Caliper* c, int scope, Snapshot* sbuf) {
    if (!fresh_sample) {
        return;
    }

    Snapshot::Sizes     sizes = sbuf->capacity();
    Snapshot::Addresses addr  = sbuf->addresses(); 

    sizes.n_nodes = 0;
    sizes.n_data  = 6;
    sizes.n_attr  = 6;

    // Attribute ids
    addr.immediate_attr[0] = address_attr.id();
    addr.immediate_attr[1] = latency_attr.id();
    addr.immediate_attr[2] = timestamp_attr.id();
    addr.immediate_attr[3] = ip_attr.id();
    addr.immediate_attr[4] = datasource_attr.id();
    addr.immediate_attr[5] = cpu_attr.id();

    // Data
    addr.immediate_data[0] = Variant(static_cast<uint64_t>(static_sample.addr));
    addr.immediate_data[1] = Variant(static_cast<uint64_t>(static_sample.weight));
    addr.immediate_data[2] = Variant(static_cast<uint64_t>(static_sample.time));
    addr.immediate_data[3] = Variant(static_cast<uint64_t>(static_sample.ip));
    addr.immediate_data[4] = Variant(static_cast<uint64_t>(static_sample.data_src));
    addr.immediate_data[5] = Variant(static_cast<uint64_t>(static_sample.cpu));

    sbuf->commit(sizes);

    fresh_sample = false;
}

void mitos_init(Caliper* c) {
    Mitos_set_sample_latency_threshold(20);
    Mitos_set_sample_time_frequency(4000);
    Mitos_set_handler_fn(&sample_handler,c);
    Mitos_begin_sampler();
}

void mitos_finish(Caliper* c) {
    cerr << "ENDING" << endl;
    Mitos_end_sampler();
}

// Initialization handler
void mitos_register(Caliper* c) {
    record_address = config.get("address").to_bool();

    address_attr = c->create_attribute("mitos.address", CALI_TYPE_ADDR, 
                                       CALI_ATTR_ASVALUE 
                                       | CALI_ATTR_SCOPE_THREAD 
                                       | CALI_ATTR_SKIP_EVENTS);
    latency_attr = c->create_attribute("mitos.latency", CALI_TYPE_ADDR, 
                                       CALI_ATTR_ASVALUE 
                                       | CALI_ATTR_SCOPE_THREAD 
                                       | CALI_ATTR_SKIP_EVENTS);
    timestamp_attr = c->create_attribute("mitos.timestamp", CALI_TYPE_ADDR, 
                                         CALI_ATTR_ASVALUE 
                                         | CALI_ATTR_SCOPE_THREAD 
                                         | CALI_ATTR_SKIP_EVENTS);
    ip_attr = c->create_attribute("mitos.ip", CALI_TYPE_ADDR, 
                                  CALI_ATTR_ASVALUE 
                                  | CALI_ATTR_SCOPE_THREAD 
                                  | CALI_ATTR_SKIP_EVENTS);
    datasource_attr = c->create_attribute("mitos.datasource", CALI_TYPE_ADDR, 
                                  CALI_ATTR_ASVALUE 
                                  | CALI_ATTR_SCOPE_THREAD 
                                  | CALI_ATTR_SKIP_EVENTS);
    cpu_attr = c->create_attribute("mitos.cpu", CALI_TYPE_ADDR, 
                                  CALI_ATTR_ASVALUE 
                                  | CALI_ATTR_SCOPE_THREAD 
                                  | CALI_ATTR_SKIP_EVENTS);

    // add callback for Caliper::get_context() event
    c->events().post_init_evt.connect(&mitos_init);
    c->events().finish_evt.connect(&mitos_finish);
    c->events().snapshot.connect(&push_sample);

    Log(1).stream() << "Registered mitos service" << endl;
}

} // namespace


namespace cali 
{
    CaliperService MitosService = { "mitos", ::mitos_register };
} // namespace cali

