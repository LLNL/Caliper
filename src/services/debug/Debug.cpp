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

/// @file  Debug.cpp
/// @brief Caliper debug service

#include "../CaliperService.h"

#include <Caliper.h>
#include <EntryList.h>

#include <csv/CsvSpec.h>
#include <Log.h>

#include <mutex>

using namespace cali;
using namespace std;

namespace
{

mutex dbg_mutex;

void create_attr_cb(Caliper* c, const Attribute& attr)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(2).stream() << "Event: create_attribute (attr=" << attr.name() << ")" << endl;
}

void begin_cb(Caliper* c, const Attribute& attr, const Variant& value)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(2).stream() << "Event: pre_begin (" << attr.name() << "=" << value << ")" << endl;
}

void end_cb(Caliper* c, const Attribute& attr, const Variant& value)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(2).stream() << "Event: pre_end ("   << attr.name() << "=" << value << ")" << endl;
}

void set_cb(Caliper* c, const Attribute& attr, const Variant& value)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(2).stream() << "Event: pre_set ("   << attr.name() << "=" << value << ")" << endl;
}

const char* scopestrings[] = { "", "process", "thread", "", "task" };

string scope2string(int scope)
{
    const cali_context_scope_t scopes[] = { 
        CALI_SCOPE_TASK, CALI_SCOPE_THREAD, CALI_SCOPE_PROCESS 
    };

    string out;

    for (cali_context_scope_t s : scopes)
        if (scope & s) {
            if (out.size())
                out.append(":");
            out.append(scopestrings[s]);
        }

    return out;
}

string format_triggerinfo(const Entry* entry)
{
    string out("trigger=");

    if (entry)
        out.append(std::to_string(static_cast<uint64_t>(entry->attribute())));
    else
        out.append("UNKNOWN");

    return out;
}

void create_scope_cb(Caliper* c, cali_context_scope_t scope)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(2).stream() << "Event: create_scope (scope=" << scope2string(scope) << ")" << endl;
}

void release_scope_cb(Caliper* c, cali_context_scope_t scope)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(2).stream() << "Event: release_scope (scope=" << scope2string(scope) << ")" << endl;
}

void snapshot_cb(Caliper* c, int scope, const EntryList* trigger_info, EntryList*)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(2).stream() << "Event: snapshot (scope=" << scope2string(scope) << ", "
                    << ")" << endl;
}

void process_snapshot_cb(Caliper* c, const EntryList* trigger_info, const EntryList* sbuf)
{
    lock_guard<mutex> lock(dbg_mutex);

    auto write_rec_fn = [](const RecordDescriptor& rec, const int count[], const Variant* data[]) {
        CsvSpec::write_record(Log(2).stream() << "Event: process_snapshot: ", rec, count, data);
    };

    sbuf->push_record(write_rec_fn);
}

void finish_cb(Caliper* c)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(2).stream() << "Event: finish" << endl;
}

void debug_service_register(Caliper* c)
{
    c->events().create_attr_evt.connect(&create_attr_cb);
    c->events().pre_begin_evt.connect(&begin_cb);
    c->events().pre_end_evt.connect(&end_cb);
    c->events().pre_set_evt.connect(&set_cb);
    c->events().finish_evt.connect(&finish_cb);
    c->events().create_scope_evt.connect(&create_scope_cb);
    c->events().release_scope_evt.connect(&release_scope_cb);
    c->events().snapshot.connect(&snapshot_cb);
    c->events().process_snapshot.connect(&process_snapshot_cb);

    Log(1).stream() << "Registered debug service" << endl;
}

} // namespace

namespace cali
{
    CaliperService DebugService = { "debug", ::debug_service_register };
}
