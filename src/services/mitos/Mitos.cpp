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
#include <SnapshotRecord.h>

#include <RuntimeConfig.h>
#include <ContextRecord.h>
#include <Log.h>

#include <Mitos.h>

using namespace cali;
using namespace std;

namespace 
{

#define MITOS_NUM_ATTR 6
    
Attribute address_attr        { Attribute::invalid } ;
Attribute latency_attr        { Attribute::invalid } ;
Attribute timestamp_attr      { Attribute::invalid } ;
Attribute ip_attr             { Attribute::invalid } ;
Attribute datasource_attr     { Attribute::invalid } ;
Attribute cpu_attr            { Attribute::invalid } ;

int       num_samples;
int       num_processed_samples;
    
ConfigSet config;

cali_id_t mitos_attributes[MITOS_NUM_ATTR] = { CALI_INV_ID };

    
const ConfigSet::Entry configdata[] = {
    { "latency_threshold", CALI_TYPE_UINT, "20",
      "Load latency threshold",
      "Latency above which samples will be collected."
    },
    { "time_frequency",    CALI_TYPE_UINT, "4000",
      "Sample frequency",
      "Number of samples per second to collect (approximately)."
    },
    ConfigSet::Terminator
};

    
void sample_handler(perf_event_sample *sample, void *args) {
    ++num_samples;

    Caliper c = Caliper::sigsafe_instance();

    if (!c)
        return;

    // Push context to invoke push_sample
    
    Variant data[MITOS_NUM_ATTR] = {
        Variant(static_cast<uint64_t>(sample->addr)),
        Variant(static_cast<uint64_t>(sample->weight)),
        Variant(static_cast<uint64_t>(sample->time)),
        Variant(static_cast<uint64_t>(sample->ip)),
        Variant(static_cast<uint64_t>(sample->data_src)),
        Variant(static_cast<uint64_t>(sample->cpu))
    };

    SnapshotRecord trigger_info(MITOS_NUM_ATTR, mitos_attributes, data);

    c.push_snapshot(CALI_SCOPE_THREAD, &trigger_info);

    ++num_processed_samples;
}

void mitos_init_thread(Caliper*, cali_context_scope_t scope)
{
    if (scope == CALI_SCOPE_THREAD) {
        Mitos_set_sample_latency_threshold(config.get("latency_threshold").to_uint());
        Mitos_set_sample_time_frequency(config.get("time_frequency").to_uint());
        Mitos_set_handler_fn(&sample_handler, nullptr);
        
        Mitos_begin_sampler();
    }
}

void mitos_end_thread(Caliper*, cali_context_scope_t scope)
{
    if (scope == CALI_SCOPE_THREAD)
        Mitos_end_sampler();
}

void mitos_init(Caliper* c)
{
    mitos_init_thread(c, CALI_SCOPE_THREAD);
}

void mitos_finish(Caliper* c)
{
    mitos_end_thread(c, CALI_SCOPE_THREAD);
    
    Log(1).stream() << "Mitos: processed " << num_processed_samples
                    << " samples ("  << num_samples
                    << " total, "    << num_samples - num_processed_samples
                    << " dropped)."  << std::endl;
}

    
// Initialization handler
void mitos_register(Caliper* c)
{
    config = RuntimeConfig::init("mitos", configdata);

    num_samples           = 0;
    num_processed_samples = 0;
    
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
    
    // Attribute ids
    
    mitos_attributes[0] = address_attr.id();
    mitos_attributes[1] = latency_attr.id();
    mitos_attributes[2] = timestamp_attr.id();
    mitos_attributes[3] = ip_attr.id();
    mitos_attributes[4] = datasource_attr.id();
    mitos_attributes[5] = cpu_attr.id();

    // Add callback for Caliper::get_context() event
    
    c->events().post_init_evt.connect(&mitos_init);
    c->events().finish_evt.connect(&mitos_finish);
    c->events().create_scope_evt.connect(&mitos_init_thread);
    c->events().release_scope_evt.connect(&mitos_end_thread);

    Log(1).stream() << "Registered mitos service" << endl;
}

} // namespace


namespace cali 
{
    CaliperService mitos_service = { "mitos", ::mitos_register };
} // namespace cali

