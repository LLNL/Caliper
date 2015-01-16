///@file  OmptService.cpp
///@brief Service for OpenMP Tools interface 

#include "../CaliperService.h"

#include <Caliper.h>
#include <SigsafeRWLock.h>

#include <Log.h>
#include <RuntimeConfig.h>

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
    ConfigSet::Terminator
};

bool                             enable_ompt { false };
Attribute                        thread_attr { Attribute::invalid };
Attribute                        state_attr  { Attribute::invalid };

SigsafeRWLock                    thread_env_lock;
map<ompt_thread_id_t, cali_id_t> thread_env; ///< Thread ID -> Environment ID

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
    Caliper*  c = Caliper::instance();

    if (config.get("environment_mapping").to_bool() == true) {
        // Create a new Caliper environment for each thread. 
        // Record thread id -> environment id mapping for later use in get_environment()

        cali_id_t env_id = 0;

        if (type == ompt_thread_initial)
            env_id = c->default_environment(CALI_SCOPE_THREAD);
        else
            env_id = c->create_environment();

        thread_env_lock.wlock();
        thread_env.insert(make_pair(thread_id, env_id));
        thread_env_lock.unlock();
    }

    // Set the thread id in the new environment

    uint64_t buf = (uint64_t) thread_id;
    c->set(thread_attr, &buf, sizeof(buf));
}

// ompt_event_thread_end

void
cb_event_thread_end(ompt_thread_type_t type, ompt_thread_id_t id)
{
    // Oops, Caliper::close_environment() is not implemented yet. Nothing to do here.
}

// ompt_event_control

void
cb_event_control(uint64_t command, uint64_t modifier)
{
    // Should react to enable / disable measurement commands.
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

cali_id_t
get_thread_environment() 
{
    cali_id_t env = Caliper::instance()->default_environment(CALI_SCOPE_THREAD);

    if (!api.get_thread_id)
        return env;

    ompt_thread_id_t thread_id = (*api.get_thread_id)();

    thread_env_lock.rlock();
    auto it = thread_env.find(thread_id);
    if (it != thread_env.end())
        env = it->second;
    thread_env_lock.unlock();

    return env;
}

void
query_cb(Caliper* c, cali_context_scope_t scope)
{
    if (!api.get_state || !(scope == CALI_SCOPE_THREAD || scope == CALI_SCOPE_TASK))
        return;

    auto it = runtime_states.find((*api.get_state)(NULL));

    if (it != runtime_states.end())
        c->set(state_attr, it->second.data(), it->second.size());
}


//
// --- Management
//

/// Register our callbacks with the OpenMP runtime

bool
register_ompt_callbacks()
{
    if (!api.set_callback)
        return false;

    struct callback_info_t { 
        ompt_event_t    event;
        ompt_callback_t cbptr;
    } callbacks[] = {
        { ompt_event_thread_begin,     (ompt_callback_t) &cb_event_thread_begin     },
        { ompt_event_thread_end,       (ompt_callback_t) &cb_event_thread_end       },
        { ompt_event_control,          (ompt_callback_t) &cb_event_control          }
//        { ompt_event_runtime_shutdown, (ompt_callback_t) &cb_event_runtime_shutdown }
    };

    for ( auto cb : callbacks ) 
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
        c->create_attribute("ompt.thread.id", CALI_TYPE_UINT,   CALI_ATTR_SCOPE_THREAD);
    state_attr  =
        c->create_attribute("ompt.state",     CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD);

    if (config.get("environment_mapping").to_bool() == true)
        c->set_environment_callback(CALI_SCOPE_THREAD, &get_thread_environment);

    Log(1).stream() << "Registered OMPT service" << endl;
}

}  // namespace [ anonymous ]


// The OpenMP tools interface intialization function, called by the OpenMP 
// runtime. We must register our callbacks w/ the OpenMP runtime here.
// Returns 0 if no callbacks should be registered (i.e., OMPT service was 
// disabled in Caliper configuration), or 1 if they should.

extern "C" {

int 
ompt_initialize(ompt_function_lookup_t lookup,
                const char*            runtime_version,
                unsigned int           /* ompt_version */)
{
    // Make sure Caliper is initialized & OMPT service is enabled in Caliper config

    Caliper* c = Caliper::instance();

    if (!::enable_ompt)
        return 0;

    Log(1).stream() << "Initializing OMPT interface with " << runtime_version << endl;

    // register callbacks

    if (!::api.init(lookup) || !::register_ompt_callbacks()) {
        Log(0).stream() << "Callback registration error: OMPT interface disabled" << endl;
        return 0;
    }

    if (::config.get("capture_state").to_bool() == true) {
        register_ompt_states();
        c->events().queryEvt.connect(&query_cb);
    }

    Log(1).stream() << "OMPT interface enabled." << endl;

    return 1;
}

} // extern "C"

namespace cali
{
    CaliperService OmptService = { "ompt", { &::omptservice_initialize } };
}
