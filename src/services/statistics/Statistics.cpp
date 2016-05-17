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

/// @file  Statistics.cpp
/// @brief Caliper statistics service

#include "../CaliperService.h"

#include <Caliper.h>

#include <Log.h>

#include <atomic>

using namespace cali;
using namespace std;

namespace
{

atomic<unsigned> num_attributes;
atomic<unsigned> num_snapshots;
atomic<unsigned> num_updates;
atomic<unsigned> num_scopes;

void create_attr_cb(Caliper*, const Attribute&)
{
    ++num_attributes;
}

void update_cb(Caliper*, const Attribute&,const Variant&)
{
    ++num_updates;
}

void create_scope_cb(Caliper*, cali_context_scope_t)
{
    ++num_scopes;
}

void snapshot_cb(Caliper*, int, const EntryList*, EntryList*)
{
    ++num_snapshots;
}

void finish_cb(Caliper* c)
{
    Log(1).stream() << "Statistics:"
                    << "\n     Number of additional scopes  : " << num_scopes.load() 
                    << "\n     Number of user attributes:     " << num_attributes.load()
                    << "\n     Number of context updates:     " << num_updates.load()
                    << "\n     Number of context snapshots:   " << num_snapshots.load() << endl;
}

void statistics_service_register(Caliper* c)
{
    num_attributes = 0;
    num_snapshots  = 0;
    num_updates    = 0;
    num_scopes     = 0;

    c->events().create_attr_evt.connect(&create_attr_cb);
    c->events().pre_begin_evt.connect(&update_cb);
    c->events().pre_end_evt.connect(&update_cb);
    c->events().pre_set_evt.connect(&update_cb);
    c->events().finish_evt.connect(&finish_cb);
    c->events().create_scope_evt.connect(&create_scope_cb);
    c->events().snapshot.connect(&snapshot_cb);

    Log(1).stream() << "Registered debug service" << endl;
}
    
} // namespace

namespace cali
{
    CaliperService StatisticsService = { "statistics", ::statistics_service_register };
}
