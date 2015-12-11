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

///@file  Callpath.cpp
///@brief Callpath provider for caliper records using libunwind

#include "../CaliperService.h"

#include <Caliper.h>

#include <RuntimeConfig.h>
#include <Log.h>

#include <cstring>
#include <string>
#include <type_traits>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#define MAX_PATH 40
#define NAMELEN  100

using namespace cali;
using namespace std;

namespace 
{

Attribute callpath_name_attr { Attribute::invalid};
Attribute callpath_addr_attr { Attribute::invalid };

ConfigSet config;

bool      use_name { false };
bool      use_addr { false };

unsigned  skip_frames { 0 };

static const ConfigSet::Entry s_configdata[] = {
    { "use_name", CALI_TYPE_BOOL, "false",
      "Record region names for call path.",
      "Record region names for call path. Incurs higher overhead."
    },
    { "use_address", CALI_TYPE_BOOL, "true",
      "Record region addresses for call path",
      "Record region addresses for call path"
    },
    { "skip_frames", CALI_TYPE_UINT, "10",
      "Skip this number of stack frames",
      "Skip this number of stack frames.\n"
      "Avoids recording stack frames within the caliper library"
    },
    ConfigSet::Terminator
};

void snapshot_cb(Caliper* c, int scope, const Entry*, Snapshot*)
{
    Variant v_addr[MAX_PATH];
    Variant v_name[MAX_PATH];

    char    strbuf[MAX_PATH][NAMELEN];

    // Init unwind context
    unw_context_t unw_ctx;
    unw_cursor_t  unw_cursor;

    unw_getcontext(&unw_ctx);

    if (unw_init_local(&unw_cursor, &unw_ctx) < 0) {
        Log(0).stream() << "callpath::measure_cb: error: unable to init libunwind cursor" << endl;
        return;
    }

    // skip n frames

    size_t n = 0;

    for (n = skip_frames; n > 0 && unw_step(&unw_cursor) > 0; --n)
        ;

    if (n > 0)
        return;

    while (n < MAX_PATH && unw_step(&unw_cursor) > 0) {
        // store path from top to bottom

        if (use_addr) {
            unw_word_t ip;
            unw_get_reg(&unw_cursor, UNW_REG_IP, &ip);

            uint64_t uint = ip;
            v_addr[MAX_PATH-(n+1)] = Variant(CALI_TYPE_ADDR, &uint, sizeof(uint64_t));
        }
        if (use_name) {
            unw_word_t offs;

            if (unw_get_proc_name(&unw_cursor, strbuf[n], NAMELEN, &offs) < 0)
                strncpy(strbuf[n], "UNKNOWN", NAMELEN);

            v_name[MAX_PATH-(n+1)] = Variant(CALI_TYPE_STRING, strbuf[n], strlen(strbuf[n]));
        }

        ++n;
    }

    if (n > 0) {
        if (use_addr)
            c->set_path(callpath_addr_attr, n, v_addr+(MAX_PATH-n));
        if (use_name)
            c->set_path(callpath_name_attr, n, v_name+(MAX_PATH-n));
    }
}

} // namespace 


namespace cali
{

void callpath_service_register(Caliper* c)
{
    config = RuntimeConfig::init("callpath", s_configdata);

    use_name    = config.get("use_name").to_bool();
    use_addr    = config.get("use_address").to_bool();
    skip_frames = config.get("skip_frames").to_uint();

    callpath_addr_attr = 
        c->create_attribute("callpath.address", CALI_TYPE_ADDR,   CALI_ATTR_SKIP_EVENTS);
    callpath_name_attr = 
        c->create_attribute("callpath.regname", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);    

    c->events().snapshot.connect(&snapshot_cb);

    Log(1).stream() << "Registered callpath service" << endl;
}

CaliperService CallpathService = { "callpath", callpath_service_register };

} // namespace cali
