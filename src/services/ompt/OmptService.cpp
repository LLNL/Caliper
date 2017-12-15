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

// OmptService.cpp
// Service for OpenMP Tools interface 

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <map>
#include <mutex>

#include <ompt.h>


using namespace cali;
using namespace std;

namespace 
{

//
// --- Data
//

const ConfigSet::Entry configdata[] = {
    { "environment_mapping", CALI_TYPE_BOOL, "false", 
      "Perform thread environment mapping in OMPT module",
      "Perform thread environment mapping in OMPT module.\n"
      "  Use if default thread environment mapping (e.g. through pthread service) is unavailable" 
    },
    { "capture_state", CALI_TYPE_BOOL, "true",
      "Capture the OpenMP runtime state on context queries",
      "Capture the OpenMP runtime state on context queries"
    },
    { "capture_events", CALI_TYPE_BOOL, "false",
      "Capture OpenMP events (enter/exit parallel regions, barriers, etc.)",
      "Capture OpenMP events (enter/exit parallel regions, barriers, etc.)"
    },
    ConfigSet::Terminator
};

volatile bool                    finished    { false };
bool                             enable_ompt { false };
bool				 perm_off    { false };
Attribute                        thread_attr { Attribute::invalid };
Attribute                        state_attr  { Attribute::invalid };
Attribute			 region_attr { Attribute::invalid };

std::mutex                       thread_env_lock;
map<ompt_thread_id_t, Caliper::Scope*> thread_env; ///< Thread ID -> Environment

map<ompt_state_t, string>        runtime_states;

ConfigSet                        config;


// The OMPT interface function pointers

struct OmptAPI {
    ompt_set_callback_t    set_callback    { nullptr };
    ompt_get_thread_id_t   get_thread_id   { nullptr };
    ompt_get_state_t       get_state       { nullptr };
    ompt_enumerate_state_t enumerate_state { nullptr };

    bool
    init(ompt_function_lookup_t lookup) {
        set_callback    = (ompt_set_callback_t)    (*lookup)("ompt_set_callback");
        get_thread_id   = (ompt_get_thread_id_t)   (*lookup)("ompt_get_thread_id");
        get_state       = (ompt_get_state_t)       (*lookup)("ompt_get_state");
        enumerate_state = (ompt_enumerate_state_t) (*lookup)("ompt_enumerate_state");

        if (!set_callback || !get_thread_id || !get_state || !enumerate_state)
            return false;

        return true;
    }
} api;


//
// --- OMPT Callbacks
//

// ompt_event_thread_begin

void
cb_event_thread_begin(ompt_thread_type_t type, ompt_thread_id_t thread_id)
{
    Caliper c;

    if (config.get("environment_mapping").to_bool() == true) {
        // Create a new Caliper environment for each thread. 
        // Record thread id -> environment id mapping for later use in get_environment()

        Caliper::Scope* ctx;

        if (type == ompt_thread_initial)
            ctx = c.default_scope(CALI_SCOPE_THREAD);
        else
            ctx = c.create_scope(CALI_SCOPE_THREAD);

        std::lock_guard<std::mutex>
            g(thread_env_lock);
        
        thread_env.insert(make_pair(thread_id, ctx));
    }

    // Set the thread id in the new environment

    c.set(thread_attr, Variant(static_cast<int>(thread_id)));
}

// ompt_event_thread_end

void
cb_event_thread_end(ompt_thread_type_t type, ompt_thread_id_t thread_id)
{
    if (finished || config.get("environment_mapping").to_bool() == false)
        return;

    Caliper::Scope* ctx { nullptr };

    thread_env_lock.lock();
    auto it = thread_env.find(thread_id);
    
    if (it != thread_env.end()) {
        ctx = it->second;
        thread_env.erase(it);
    }
    thread_env_lock.unlock();

    if (ctx)
        Caliper().release_scope(ctx);
}

// ompt_event_parallel_begin

void
cb_event_parallel_begin(ompt_thread_type_t type)
{
	if ( enable_ompt == true && !finished ) {
		Caliper c;
		c.begin(region_attr, Variant(CALI_TYPE_STRING, "parallel", 8));
	}	
}

// ompt_event_parallel_end

void
cb_event_parallel_end(ompt_thread_type_t type)
{
	if ( enable_ompt == true && !finished ) {
		Caliper c;
		c.end(region_attr);
	}
}

// ompt_event_idle_begin

void
cb_event_idle_begin(ompt_thread_type_t type)
{
	if ( enable_ompt == true && !finished ) {
		Caliper c;
		c.begin(region_attr, Variant(CALI_TYPE_STRING, "idle", 4));
	}	
}

// ompt_event_idle_end

void
cb_event_idle_end(ompt_thread_type_t type)
{
	if ( enable_ompt == true && !finished ) {
		Caliper c;
		c.end(region_attr);
	}
}


// ompt_event_wait_barrier_begin

void
cb_event_wait_barrier_begin(ompt_thread_type_t type, ompt_thread_id_t thread_id)
{
	if ( enable_ompt == true && !finished) {
		Caliper c;
		c.begin(region_attr, Variant(CALI_TYPE_STRING,"barrier",7));
	}	
}

// ompt_event_wait_barrier_end

void
cb_event_wait_barrier_end(ompt_thread_type_t type, ompt_thread_id_t thread_id)
{
	if ( enable_ompt == true && !finished) {
		Caliper c;
		c.end(region_attr);
	}
}

// ompt_event_control

void
cb_event_control(uint64_t command, uint64_t modifier)
{
    // Should react to enable / disable measurement commands.
    switch (command)
    {
	    case 1 : // Start or restart monitoring
		if ( perm_off == false && enable_ompt == false) {
			enable_ompt = true;
		}
		break;
	    case 2 : // Pause monitoring
		if ( enable_ompt == true ) {
			enable_ompt = false;
		}
		break;
	    case 3 : // Flush buffers and continue monitoring
		// To be iplemented if a case arises where we would want to do this.
		break;
	    case 4 : // Permanently turn off monitoring
	    	perm_off = true;
		enable_ompt = false;
		break;
	    default :
		break;
    }
		    
}

// ompt_event_runtime_shutdown

void
cb_event_runtime_shutdown(void)
{
    // This seems to be called after the Caliper static object has been destroyed.
    // Hence, we can't do much here.
}


//
// -- Caliper callbacks
//

void
finish_cb(Caliper*)
{
    finished = true;
}

Caliper::Scope*
get_thread_scope(Caliper* c, bool alloc) 
{
    Caliper::Scope* ctx = c->default_scope(CALI_SCOPE_THREAD);

    if (!api.get_thread_id)
        return ctx;

    ompt_thread_id_t thread_id = (*api.get_thread_id)();

    thread_env_lock.lock();
    auto it = thread_env.find(thread_id);
    if (it != thread_env.end())
        ctx = it->second;
    thread_env_lock.unlock();

    return ctx;
}

void
snapshot_cb(Caliper* c, int scope, const SnapshotRecord*, SnapshotRecord*)
{
    if (!api.get_state || !(scope & CALI_SCOPE_THREAD))
        return;

    auto it = runtime_states.find((*api.get_state)(NULL));

    if (it != runtime_states.end())
        c->set(state_attr, Variant(CALI_TYPE_STRING, it->second.data(), it->second.size()));
}


//
// --- Management
//

/// Register our callbacks with the OpenMP runtime

bool
register_ompt_callbacks(bool capture_events)
{
    if (!api.set_callback)
        return false;

    struct callback_info_t { 
        ompt_event_t    event;
        ompt_callback_t cbptr;
    } basic_callbacks[] = {
        { ompt_event_thread_begin,       (ompt_callback_t) &cb_event_thread_begin       },
        { ompt_event_thread_end,         (ompt_callback_t) &cb_event_thread_end         },
        { ompt_event_control,            (ompt_callback_t) &cb_event_control            }
//        { ompt_event_runtime_shutdown, (ompt_callback_t) &cb_event_runtime_shutdown }
    }, event_callbacks[] = {
	{ ompt_event_idle_begin,         (ompt_callback_t) &cb_event_idle_begin         },
	{ ompt_event_idle_end,           (ompt_callback_t) &cb_event_idle_end           },
	{ ompt_event_wait_barrier_begin, (ompt_callback_t) &cb_event_wait_barrier_begin },
	{ ompt_event_wait_barrier_end,   (ompt_callback_t) &cb_event_wait_barrier_end   },
	{ ompt_event_parallel_begin,     (ompt_callback_t) &cb_event_parallel_begin     },
	{ ompt_event_parallel_end,       (ompt_callback_t) &cb_event_parallel_end       }
    };

    for ( auto cb : basic_callbacks ) 
        if ((*api.set_callback)(cb.event, cb.cbptr) == 0)
            return false;
    
    if (capture_events)
        for ( auto cb : event_callbacks ) 
            if ((*api.set_callback)(cb.event, cb.cbptr) == 0)
                return false;

    return true;
}

bool 
register_ompt_states()
{
    if (!api.enumerate_state)
        return false;

    ompt_state_t state = ompt_state_first;
    const char*  state_name;

    while ((*api.enumerate_state)(state, (int*) &state, &state_name))
        runtime_states[state] = state_name;

    return true;
}


/// The Caliper service initialization callback.
/// Register attributes and set Caliper callbacks here.

void 
omptservice_initialize(Caliper* c) 
{
    config      = RuntimeConfig::init("ompt", configdata);

    enable_ompt = true;

    thread_attr = 
        c->create_attribute("ompt.thread.id", CALI_TYPE_INT, CALI_ATTR_SCOPE_THREAD);
    
    state_attr  =
        c->create_attribute("ompt.state",     CALI_TYPE_STRING, 
                            CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
    region_attr =
	c->create_attribute("ompt.region",    CALI_TYPE_STRING,
			    CALI_ATTR_SCOPE_THREAD);

    if (config.get("environment_mapping").to_bool() == true)
        c->set_scope_callback(CALI_SCOPE_THREAD, &get_thread_scope);

    c->events().finish_evt.connect(&finish_cb);

    Log(1).stream() << "Registered OMPT service" << endl;
}

}  // namespace [ anonymous ]


extern "C" {

// The OpenMP tools interface intialization function, called by the OpenMP 
// runtime. We must register our callbacks w/ the OpenMP runtime here.

void
ompt_initialize_real(ompt_function_lookup_t lookup,
                     const char*            runtime_version,
                     unsigned int           /* ompt_version */)
{
    Log(1).stream() << "Initializing OMPT interface with " << runtime_version << endl;

    Caliper c;
    
    // register callbacks

    if (!::api.init(lookup) || !::register_ompt_callbacks(::config.get("capture_events").to_bool())) {
        Log(0).stream() << "Callback registration error: OMPT interface disabled" << endl;
        return;
    }

    if (::config.get("capture_state").to_bool() == true) {
        register_ompt_states();
        c.events().snapshot.connect(&snapshot_cb);
    }

    // set default thread ID
    if (::api.get_thread_id)
        c.set(thread_attr, Variant(static_cast<int>((*api.get_thread_id)())));
        
    Log(1).stream() << "OMPT interface enabled." << endl;
}
    
// Old version of initialization interface. 

int 
ompt_initialize(ompt_function_lookup_t lookup,
                const char*            runtime_version,
                unsigned int           ompt_version)
{
    // Make sure Caliper is initialized & OMPT service is enabled in Caliper config

    Caliper c;

    if (!::enable_ompt)
        return 0;

    ompt_initialize_real(lookup, runtime_version, ompt_version);
    
    return 1;
}

// New version of the initialization interface; returns NULL or real init fn
// if we want to enable OMPT

// The tech report calls it ompt_initialize_fn_t, but the Intel header does not ...
ompt_initialize_t
ompt_tool(void)
{
    // Make sure Caliper is initialized & OMPT service is enabled    
    Caliper::instance();
    
    return ::enable_ompt ? ompt_initialize_real : NULL;
}
    
} // extern "C"

namespace cali
{
    CaliperService ompt_service = { "ompt", ::omptservice_initialize };
}
