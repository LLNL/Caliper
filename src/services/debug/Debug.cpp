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

// Debug.cpp
// Caliper debug service

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"

#include <mutex>

using namespace cali;
using namespace std;

namespace
{

mutex dbg_mutex;

void create_attr_cb(Caliper* c, Channel* chn, const Attribute& attr)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: create_attribute (attr=" << attr << ")" << endl;
}

void begin_cb(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: pre_begin (" << attr.name() << "=" << value << ")" << endl;
}

void end_cb(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: pre_end ("   << attr.name() << "=" << value << ")" << endl;
}

void set_cb(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: pre_set ("   << attr.name() << "=" << value << ")" << endl;
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

void create_thread_cb(Caliper* c, Channel* chn)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: create_thread" << endl;
}

void release_thread_cb(Caliper* c, Channel* chn)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: release_thread" << endl;
}

void snapshot_cb(Caliper* c, Channel* chn, int scope, const SnapshotRecord* trigger_info, SnapshotRecord*)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: snapshot (scope=" << scope2string(scope) << ", "
                    << ")" << endl;
}

std::ostream& print_snapshot_record(std::ostream& os, const SnapshotRecord* s)
{
    os << "{ references: ";
    
    int c = 0;
    for (size_t i = 0; i < s->num_nodes(); ++i)
        os << (c++ > 0 ? "," : "") << s->data().node_entries[i]->id();

    os << "; immediate: ";
    
    c = 0;
    for (size_t i = 0; i < s->num_immediate(); ++i)
        os << (c++ > 0 ? "," : "")
           << s->data().immediate_attr[i] << ":"
           << s->data().immediate_data[i];

    return (os << " }");
}
    
void process_snapshot_cb(Caliper* c, Channel* chn, const SnapshotRecord* trigger_info, const SnapshotRecord* sbuf)
{
    lock_guard<mutex> lock(dbg_mutex);    

    print_snapshot_record( Log(1).stream() << chn->name() << ": Event: process_snapshot: ", sbuf ) << std::endl;
}

void finish_cb(Caliper* c, Channel* chn)
{
    lock_guard<mutex> lock(dbg_mutex);
    Log(1).stream() << chn->name() << ": Event: finish" << endl;
}

void debug_service_register(Caliper* c, Channel* chn)
{
    chn->events().create_attr_evt.connect(&create_attr_cb);
    chn->events().pre_begin_evt.connect(&begin_cb);
    chn->events().pre_end_evt.connect(&end_cb);
    chn->events().pre_set_evt.connect(&set_cb);
    chn->events().finish_evt.connect(&finish_cb);
    chn->events().create_thread_evt.connect(&create_thread_cb);
    chn->events().release_thread_evt.connect(&release_thread_cb);
    chn->events().snapshot.connect(&snapshot_cb);
    chn->events().process_snapshot.connect(&process_snapshot_cb);

    Log(1).stream() << chn->name() << ": Registered debug service" << endl;
}

} // namespace

namespace cali
{

CaliperService debug_service = { "debug", ::debug_service_register };

}
