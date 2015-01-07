///@file  OmptService.cpp
///@brief Service for OpenMP Tools interface 

#include "../CaliperService.h"

#include <Caliper.h>
#include <SigsafeRWLock.h>

#include <Log.h>

#include <ompt.h>


using namespace cali;
using namespace std;

namespace 
{

//
// --- Data
//

bool                             enable_ompt { false };
Attribute                        thread_attr { Attribute::invalid };

SigsafeRWLock                    thread_env_lock;
map<ompt_thread_id_t, cali_id_t> thread_env; ///< Thread ID -> Environment ID


// The OMPT interface function pointers

typedef int             (*set_callback_fptr_t) (ompt_event_t, ompt_callback_t);
typedef ompt_thread_id_t(*get_thread_id_fptr_t)(void);

struct ompt_fn_t {
    set_callback_fptr_t  set_callback;
    get_thread_id_fptr_t get_thread_id;

    bool
    init(ompt_function_lookup_t lookup) {
        set_callback  = (set_callback_fptr_t)  (*lookup)("ompt_set_callback");
        get_thread_id = (get_thread_id_fptr_t) (*lookup)("ompt_get_thread_id");

        if (!set_callback || !get_thread_id)
            return false;

        return true;
    }
} ompt_fn;


//
// --- OMPT Callbacks
//

// ompt_event_thread_begin

void
cb_event_thread_begin(ompt_thread_type_t type, ompt_thread_id_t thread_id)
{
    // Create a new Caliper environment for each thread. 
    // Record thread id -> environment id mapping for later use in get_environment()

    Caliper*  c = Caliper::instance();
    cali_id_t env_id = 0;

    if (type == ompt_thread_initial)
        env_id = 0;
    else
        // Clone default environment
        env_id = c->clone_environment(0);

    thread_env_lock.wlock();
    thread_env.insert(make_pair(thread_id, env_id));
    thread_env_lock.unlock();

    // Set the thread id in the new environment

    uint64_t buf = (uint64_t) thread_id;
    c->set(env_id, thread_attr, &buf, sizeof(buf));
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


//
// -- Caliper callbacks
//

cali_id_t
get_environment() 
{
    if (!ompt_fn.get_thread_id)
        return 0;

    cali_id_t        env       = 0;
    ompt_thread_id_t thread_id = (*ompt_fn.get_thread_id)();

    // FIXME: use some sort of thread-local storage to avoid map lookup & lock
    thread_env_lock.rlock();

    auto it = thread_env.find(thread_id);
    if (it != thread_env.end())
        env = it->second;

    thread_env_lock.unlock();

    return env;
}


//
// --- Management
//

/// Register our callbacks with the OpenMP runtime

bool
register_ompt_callbacks()
{
    if (!ompt_fn.set_callback)
        return false;

    struct callback_info_t { 
        ompt_event_t    event;
        ompt_callback_t cbptr;
    } callbacks[] = {
        { ompt_event_thread_begin, (ompt_callback_t) &cb_event_thread_begin },
        { ompt_event_thread_end,   (ompt_callback_t) &cb_event_thread_end   },
        { ompt_event_control,      (ompt_callback_t) &cb_event_control      }
    };

    for ( auto cb : callbacks ) 
        if ((*ompt_fn.set_callback)(cb.event, cb.cbptr) == 0)
            return false;

    return true;
}

/// The Caliper service initialization callback.
/// Register attributes and set Caliper callbacks here.

void 
omptservice_initialize(Caliper* c) 
{
    enable_ompt = true;
    thread_attr = c->create_attribute("thread", CALI_TYPE_UINT, CALI_ATTR_ASVALUE);

    c->set_environment_callback(&get_environment);
}
    
}  // namespace 


// The OpenMP tools interface intialization function, called by the OpenMP 
// runtime. We must register our callbacks w/ the OpenMP runtime here.
// Returns 0 if no callbacks should be registered (i.e., OMPT service was 
// disabled in Caliper configuration), or 1 if they should.

extern "C" {

int 
ompt_initialize(ompt_function_lookup_t lookup,
                const char*            runtime_version,
                unsigned int           ompt_version)
{
    // Make sure Caliper is initialized & OMPT service is enabled in Caliper config

    Caliper::instance();

    if (!::enable_ompt)
        return 0;

    Log(1).stream() << "Initializing OMPT interface v" << ompt_version
                    << " with " << runtime_version << endl;

    // register callbacks

    if (!::ompt_fn.init(lookup) || !::register_ompt_callbacks()) {
        Log(0).stream() << "Callback registration error: OMPT interface disabled" << endl;
        return 0;
    }

    Log(1).stream() << "OMPT callbacks enabled." << endl;

    return 1;
}

}


namespace cali
{
    CaliperService OmptService = { "ompt", { &::omptservice_initialize } };
}
