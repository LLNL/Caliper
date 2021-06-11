// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/Caliper.h"

#include "caliper/CaliperService.h"

#include "caliper/common/Log.h"
#include "caliper/common/StringConverter.h"

#include <ompt.h>

#include <cstdlib>

using namespace cali;

namespace
{

struct OmptAPI {
    ompt_set_callback_t      set_callback     { nullptr };
    ompt_get_state_t         get_state        { nullptr };
    ompt_enumerate_states_t  enumerate_states { nullptr };
    ompt_get_proc_id_t       get_proc_id      { nullptr };
    ompt_finalize_tool_t     finalize_tool    { nullptr };

    bool
    init(ompt_function_lookup_t lookup) {
        set_callback     = (ompt_set_callback_t)     (*lookup)("ompt_set_callback");
        get_state        = (ompt_get_state_t)        (*lookup)("ompt_get_state");
        enumerate_states = (ompt_enumerate_states_t) (*lookup)("ompt_enumerate_states");
        get_proc_id      = (ompt_get_proc_id_t)      (*lookup)("ompt_get_proc_id");
        finalize_tool    = (ompt_finalize_tool_t)    (*lookup)("ompt_finalize_tool");

        if (!set_callback || !get_state || !enumerate_states || !get_proc_id || !finalize_tool)
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
Attribute    thread_id_attr     { Attribute::invalid };
Attribute    num_threads_attr   { Attribute::invalid };

unsigned int num_skipped        { 0 };

//
// --- The OMPT callbacks
//

void cb_thread_begin(ompt_thread_t type, ompt_data_t*)
{
    Caliper c;

    if (!c) {
        ++num_skipped;
        return;
    }

    int proc_id = (*api.get_proc_id)();

    if (proc_id >= 0)
        c.begin(proc_id_attr, Variant(proc_id));

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
    Caliper c;

    if (!c) {
        ++num_skipped;
        return;
    }

    if (!c.get(thread_type_attr).is_empty())
        c.end(thread_type_attr);
    if (!c.get(proc_id_attr).is_empty())
        c.end(proc_id_attr);
}

void cb_parallel_begin(ompt_data_t*, ompt_frame_t*, ompt_data_t*, unsigned nthreads, int, const void*)
{
    Caliper c;

    if (!c) {
        ++num_skipped;
        return;
    }

    c.begin(region_attr, Variant(cali_make_variant_from_uint(nthreads)));
}

void cb_parallel_end(ompt_data_t*, ompt_data_t*, int, const void*)
{
    Caliper c;

    if (!c) {
        ++num_skipped;
        return;
    }

    c.end(region_attr);
}

void cb_implicit_task(ompt_scope_endpoint_t endpoint, ompt_data_t*, ompt_data_t*, unsigned int par, unsigned int index, int flags)
{
    // ignore the initial task
    if (flags & ompt_task_initial)
        return;

    Caliper c;

    if (!c) {
        ++num_skipped;
        return;
    }

    if (endpoint == ompt_scope_begin) {
        c.begin(num_threads_attr, cali_make_variant_from_int(static_cast<int>(par)));
        c.begin(thread_id_attr, cali_make_variant_from_int(static_cast<int>(index)));
    } else {
        c.end(thread_id_attr);
        c.end(num_threads_attr);
    }
}

void cb_work(int wstype, ompt_scope_endpoint_t endpoint, ompt_data_t*, ompt_data_t*, uint64_t, const void*)
{
    const char* work_region_names[] = {
        "UNKNOWN",
        "loop",
        "sections",
        "single_executor",
        "single_other",
        "workshare",
        "distribute",
        "taskloop",
        "scope"
    };

    const char* name = "UNKNOWN";

    if (wstype > 0 && wstype <= 8)
        name = work_region_names[wstype];

    Caliper c;

    if (!c) {
        ++num_skipped;
        return;
    }

    if (endpoint == ompt_scope_begin) {
        c.begin(work_attr, name);
    } else if (endpoint == ompt_scope_end) {
        c.end(work_attr);
    }
}

void cb_sync_region(int kind, ompt_scope_endpoint_t endpoint, ompt_data_t*, ompt_data_t*, const void*)
{
    const char* sync_region_names[] = {
        "UNKNOWN",
        "barrier",
        "barrier_implicit",
        "barrier_explicit",
        "barrier_implementation",
        "taskwait",
        "taskgroup",
        "reduction",
        "barrier_implicit_workshare",
        "barrier_implicit_parallel",
        "barrier_teams"
    };

    const char* name = "UNKNOWN";

    if (kind > 0 && kind <= 10)
        name = sync_region_names[kind];

    Caliper c;

    if (!c) {
        ++num_skipped;
        return;
    }

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
        { ompt_callback_sync_region,    reinterpret_cast<ompt_callback_t>(cb_sync_region)    },
        { ompt_callback_implicit_task,  reinterpret_cast<ompt_callback_t>(cb_implicit_task)  }
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

    return 42; // return a non-zero
}

void finalize_ompt(ompt_data_t* tool_data)
{
    // This can be invoked after Caliper destruction so use cerr instead of our own log
    if (num_skipped > 0)
        std::cerr << "== CALIPER: OMPT: " << num_skipped << " callbacks skipped" << std::endl;
}

ompt_start_tool_result_t start_tool_result { initialize_ompt, finalize_ompt, { 0 } };


//
// --- Caliper management
//

void post_init_cb(Caliper* c, Channel* channel)
{
    const Attribute sub_attrs[] = {
        region_attr, thread_type_attr, sync_attr, work_attr, thread_id_attr
    };

    for (const Attribute& attr : sub_attrs)
        channel->events().subscribe_attribute(c, channel, attr);
}

void create_attributes(Caliper* c)
{
    Attribute subscription_attr = c->get_attribute("subscription_event");
    Variant v_true(true);

    region_attr =
        c->create_attribute("omp.parallel", CALI_TYPE_UINT,
                            CALI_ATTR_SCOPE_THREAD,
                            1, &subscription_attr, &v_true);
    thread_type_attr =
        c->create_attribute("omp.thread.type", CALI_TYPE_STRING,
                            CALI_ATTR_SCOPE_THREAD |
                            CALI_ATTR_UNALIGNED,
                            1, &subscription_attr, &v_true);
    sync_attr =
        c->create_attribute("omp.sync", CALI_TYPE_STRING,
                            CALI_ATTR_SCOPE_THREAD,
                            1, &subscription_attr, &v_true);
    work_attr =
        c->create_attribute("omp.work", CALI_TYPE_STRING,
                            CALI_ATTR_SCOPE_THREAD,
                            1, &subscription_attr, &v_true);
    state_attr =
        c->create_attribute("omp.state", CALI_TYPE_STRING,
                            CALI_ATTR_SCOPE_THREAD |
                            CALI_ATTR_SKIP_EVENTS);
    proc_id_attr =
        c->create_attribute("omp.proc.id", CALI_TYPE_INT,
                            CALI_ATTR_SCOPE_THREAD |
                            CALI_ATTR_UNALIGNED    |
                            CALI_ATTR_SKIP_EVENTS);
    thread_id_attr =
        c->create_attribute("omp.thread.id", CALI_TYPE_INT,
                            CALI_ATTR_SKIP_EVENTS  |
                            CALI_ATTR_UNALIGNED    |
                            CALI_ATTR_SCOPE_THREAD);
    num_threads_attr =
        c->create_attribute("omp.num.threads", CALI_TYPE_INT,
                            CALI_ATTR_SCOPE_THREAD |
                            CALI_ATTR_UNALIGNED    |
                            CALI_ATTR_SKIP_EVENTS);
}

int num_ompt_channels = 0;

void pre_finish_cb(Caliper*, Channel* channel) {
    if (--num_ompt_channels == 0) {
        Log(1).stream() << channel->name() << ": Finalizing OMPT" << std::endl;

        if (api.finalize_tool)
            (*api.finalize_tool)();
        else {
            Log(0).stream() << channel->name()
                            << ": ompt: OMPT support was not enabled: "
                               "Set the CALI_USE_OMPT environment variable to enable it (CALI_USE_OMPT=1)"
                            << std::endl;
        }
    }
}

void register_ompt_service(Caliper* c, Channel* channel)
{
    static bool is_initialized = false;

    if (!is_initialized) {
        is_initialized = true;
        create_attributes(c);
    }

    ++num_ompt_channels;

    channel->events().post_init_evt.connect(post_init_cb);
    channel->events().pre_finish_evt.connect(pre_finish_cb);

    Log(1).stream() << channel->name() << ": " << "Registered OMPT service" << std::endl;
}

} // namespace [anonymous]

extern "C" {

ompt_start_tool_result_t*
ompt_start_tool(unsigned omp_version, const char* runtime_version) {
    bool use_ompt = (::num_ompt_channels > 0);

    const char* optstr = std::getenv("CALI_USE_OMPT");

    if (optstr)
        use_ompt = StringConverter(optstr).to_bool();

    if (Log::verbosity() >= 2)
        Log(2).stream() << "OMPT is available. Using " << runtime_version
                        << ". OMPT requested: "
                        << (use_ompt ? "Yes" : "No") << std::endl;

    return use_ompt ? &::start_tool_result : nullptr;
}

}

namespace cali
{

CaliperService ompt_service { "ompt", ::register_ompt_service };

}
