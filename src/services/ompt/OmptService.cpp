// Copyright (c) 2020, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

#include "caliper/Caliper.h"

#include "caliper/CaliperService.h"

#include "caliper/common/Log.h"

#include <ompt.h>

#include <cstdlib>

using namespace cali;

namespace 
{

struct OmptAPI {
    ompt_set_callback_t      set_callback     { nullptr };
    ompt_get_state_t         get_state        { nullptr };
    ompt_enumerate_states_t  enumerate_states { nullptr };

    // std::vector<std::string> states;

    bool
    init(ompt_function_lookup_t lookup) {
        set_callback     = (ompt_set_callback_t)     (*lookup)("ompt_set_callback");
        get_state        = (ompt_get_state_t)        (*lookup)("ompt_get_state");
        enumerate_states = (ompt_enumerate_states_t) (*lookup)("ompt_enumerate_states");

        if (!set_callback || !get_state || !enumerate_states)
            return false;

        // enumerate states

        // states.reserve(32);

        // int state = ompt_state_undefined;
        // const char* state_name;

        // while ((*api.enumerate_states)(state, &state, &state_name)) {
        //     if (states.size() <= state)
        //         states.resize(state);

        //     states[state] = std::string(state_name);
        // }

        return true;
    }
} api;

Attribute    region_attr        { Attribute::invalid };
Attribute    sync_attr          { Attribute::invalid };
Attribute    work_attr          { Attribute::invalid };
Attribute    thread_type_attr   { Attribute::invalid };
Attribute    state_attr         { Attribute::invalid };
Attribute    proc_id_attr       { Attribute::invalid };

// 
// --- The OMPT callbacks
//

void cb_thread_begin(ompt_thread_t type, ompt_data_t*)
{
    Caliper c;

    switch (type) {
    case ompt_thread_initial:  
        c.begin(thread_type_attr, Variant("initial"));
        break;
    case ompt_thread_worker:
        c.begin(thread_type_attr, Variant("worker"));
        break;
    case ompt_thread_other:
        c.begin(thread_type_attr, Variant("other"));
        break;
    default:
        c.begin(thread_type_attr, Variant("unknown"));
    }
}

void cb_thread_end(ompt_data_t*)
{
    Caliper c = Caliper::sigsafe_instance();

    if (c && !c.get(thread_type_attr).is_empty())
        c.end(thread_type_attr);
}

void cb_parallel_begin(ompt_data_t*, ompt_frame_t*, ompt_data_t*, unsigned, int, const void*)
{
    Caliper c;
    c.begin(region_attr, Variant("parallel"));
}

void cb_parallel_end(ompt_data_t*, ompt_data_t*, int, const void*)
{
    Caliper c;
    c.end(region_attr);
}

void cb_work(int wstype, ompt_scope_endpoint_t endpoint, ompt_data_t*, ompt_data_t*, uint64_t, const void*)
{
    const struct work_info_t {
        int w; const char* name;
    } work_info[] = {
        { 0,                  "UNKNOWN"  },
        { ompt_work_loop,     "loop"     },
        { ompt_work_sections, "sections" },
        { ompt_work_single_executor, "single_executor" },
        { ompt_work_single_other, "single_other" },
        { ompt_work_workshare, "workshare" },
        { ompt_work_taskloop, "taskloop" }
    };

    const char* name = (std::max(wstype, 0) > ompt_work_taskloop ? "UNKNOWN" : work_info[wstype].name);

    Caliper c;

    if (endpoint == ompt_scope_begin) {
        c.begin(work_attr, name);
    } else if (endpoint == ompt_scope_end) {
        c.end(work_attr);
    }
}

void cb_sync_region(int kind, ompt_scope_endpoint_t endpoint, ompt_data_t*, ompt_data_t*, const void*)
{
    const struct sync_info_t {
        int s; const char* name;
    } sync_info[] {
        { ompt_sync_region_barrier,   "barrier"   },
        { ompt_sync_region_barrier_implicit, "barrier_implicit" },
        { ompt_sync_region_barrier_explicit, "barrier_explicit" },
        { ompt_sync_region_barrier_implementation, "barrier_implementation" },
        { ompt_sync_region_taskwait,  "taskwait"  },
        { ompt_sync_region_taskgroup, "taskgroup" },
        { ompt_sync_region_reduction, "reduction" }
    };

    const char* name = "UNKNOWN";

    for (auto si : sync_info)
        if (kind == si.s) {
            name = si.name;
            break;
        }

    Caliper c;

    if (endpoint == ompt_scope_begin)
        c.begin(sync_attr, Variant(name));
    else if (endpoint == ompt_scope_end)
        c.end(sync_attr);
}

//
// --- OMPT management
//


void setup_ompt_callbacks()
{
    const struct callback_info {
        ompt_callbacks_t cb;
        ompt_callback_t  fn;
    } callbacks[] = {
        { ompt_callback_thread_begin,   reinterpret_cast<ompt_callback_t>(cb_thread_begin)   },
        { ompt_callback_thread_end,     reinterpret_cast<ompt_callback_t>(cb_thread_end)     },
        { ompt_callback_parallel_begin, reinterpret_cast<ompt_callback_t>(cb_parallel_begin) },
        { ompt_callback_parallel_end,   reinterpret_cast<ompt_callback_t>(cb_parallel_end)   },
        { ompt_callback_work,           reinterpret_cast<ompt_callback_t>(cb_work)           },
        { ompt_callback_sync_region,    reinterpret_cast<ompt_callback_t>(cb_sync_region)    }
    };

    for (auto info : callbacks)
        (*(api.set_callback))(info.cb, info.fn);
}

int initialize_ompt(ompt_function_lookup_t lookup, int initial_device_num, ompt_data_t* tool_data) 
{
    if (!api.init(lookup)) {
        Log(0).stream() << "Cannot initialize OMPT API" << std::endl;
        return 0;
    }

    setup_ompt_callbacks();

    Log(1).stream() << "OMPT support initialized" << std::endl;

    return 42;
}

void finalize_ompt(ompt_data_t* tool_data) 
{

}

ompt_start_tool_result_t start_tool_result { initialize_ompt, finalize_ompt, { 0 } };


//
// --- Caliper management
//

void post_init_cb(Caliper* c, Channel* channel)
{
    const Attribute sub_attrs[] = {
        region_attr, thread_type_attr, sync_attr, work_attr
    };

    for (const Attribute& attr : sub_attrs)
        channel->events().subscribe_attribute(c, channel, attr);
}

void create_attributes(Caliper* c)
{
    Attribute subscription_attr = c->get_attribute("subscription_event");
    Variant v_true(true);

    struct attribute_info_t {
        Attribute*  attr;
        const char* name;
    } attr_info[] = {
        { &region_attr,      "omp.region" },
        { &thread_type_attr, "omp.thread.type" },
        { &sync_attr, "omp.sync" },
        { &work_attr, "omp.work" },
    };

    for (attribute_info_t& info : attr_info) 
        *(info.attr) = 
            c->create_attribute(info.name, CALI_TYPE_STRING, 
                                CALI_ATTR_SCOPE_THREAD, 
                                1, &subscription_attr, &v_true);

    state_attr =
        c->create_attribute("omp.state",       CALI_TYPE_STRING, 
                            CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
    proc_id_attr =
        c->create_attribute("omp.proc.id",     CALI_TYPE_INT, 
                            CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
}

void register_ompt_service(Caliper* c, Channel* channel)
{
    static bool is_initialized = false;

    if (!is_initialized) {
        is_initialized = true;
        create_attributes(c);
    }

    channel->events().post_init_evt.connect(post_init_cb);

    Log(1).stream() << channel->name() << ": " << "Registered OMPT service" << std::endl;
}

} // namespace [anonymous]

extern "C" {

ompt_start_tool_result_t*
ompt_start_tool(unsigned omp_version, const char* runtime_version) {
    Log(2).stream() << "OMPT is available. Using " << runtime_version << std::endl;

    return &::start_tool_result;
}

}

namespace cali
{

CaliperService ompt_service { "ompt", ::register_ompt_service };

}
