///@file  OmptService.cpp
///@brief Service for OpenMP Tools interface 

#include "../CaliperService.h"

#include <Caliper.h>
#include <SigsafeRWLock.h>

#include <Log.h>
#include <RuntimeConfig.h>

#define CALIPER_OMPT_USE_PTHREAD_TLS

#ifdef CALIPER_OMPT_USE_PTHREAD_TLS
#include <pthread.h>
#include <cstdlib>
#endif

#include <ompt.h>


using namespace cali;
using namespace std;

namespace 
{

//
// --- Data
//

const ConfigSet::Entry configdata[] = {
    ConfigSet::Terminator
};

bool                             enable_ompt { false };
Attribute                        thread_attr { Attribute::invalid };

#ifdef CALIPER_OMPT_USE_PTHREAD_TLS
pthread_key_t                    thread_env_key;
#else
SigsafeRWLock                    thread_env_lock;
map<ompt_thread_id_t, cali_id_t> thread_env; ///< Thread ID -> Environment ID
#endif

ConfigSet                        config;


// The OMPT interface function pointers

struct OmptAPI {
    ompt_set_callback_t  set_callback  { nullptr };
    ompt_get_thread_id_t get_thread_id { nullptr };

    bool
    init(ompt_function_lookup_t lookup) {
        set_callback  = (ompt_set_callback_t)  (*lookup)("ompt_set_callback");
        get_thread_id = (ompt_get_thread_id_t) (*lookup)("ompt_get_thread_id");

        if (!set_callback || !get_thread_id)
            return false;

        return true;
    }
} omptapi;


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

#ifdef CALIPER_OMPT_USE_PTHREAD_TLS
    cali_id_t* ptr = static_cast<cali_id_t*>(malloc(sizeof(cali_id_t)));
    *ptr = env_id;
    pthread_setspecific(thread_env_key, ptr);
#else
    thread_env_lock.wlock();
    thread_env.insert(make_pair(thread_id, env_id));
    thread_env_lock.unlock();
#endif

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
get_environment() 
{
    if (!omptapi.get_thread_id)
        return 0;

    cali_id_t        env       = 0;
    ompt_thread_id_t thread_id = (*omptapi.get_thread_id)();

#ifdef CALIPER_OMPT_USE_PTHREAD_TLS
    cali_id_t* ptr = static_cast<cali_id_t*>(pthread_getspecific(thread_env_key));
    if (ptr)
        env = *ptr;
#else
    thread_env_lock.rlock();
    auto it = thread_env.find(thread_id);
    if (it != thread_env.end())
        env = it->second;
    thread_env_lock.unlock();
#endif

    return env;
}


//
// --- Management
//

/// Register our callbacks with the OpenMP runtime

bool
register_ompt_callbacks()
{
    if (!omptapi.set_callback)
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
        if ((*omptapi.set_callback)(cb.event, cb.cbptr) == 0)
            return false;

    return true;
}

/// The Caliper service initialization callback.
/// Register attributes and set Caliper callbacks here.

void 
omptservice_initialize(Caliper* c) 
{
    config      = RuntimeConfig::init("ompt", configdata);

    enable_ompt = true;
    thread_attr = c->create_attribute("thread", CALI_TYPE_UINT, CALI_ATTR_ASVALUE);

    c->set_environment_callback(&get_environment);
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
                unsigned int           ompt_version)
{
    // Make sure Caliper is initialized & OMPT service is enabled in Caliper config

    Caliper::instance();

    if (!::enable_ompt)
        return 0;

    Log(1).stream() << "Initializing OMPT interface v" << ompt_version
                    << " with " << runtime_version << endl;

    // initialize thread-local storage key
#ifdef CALIPER_OMPT_USE_PTHREAD_TLS
    pthread_key_create(&thread_env_key, &std::free);
#endif

    // register callbacks

    if (!::omptapi.init(lookup) || !::register_ompt_callbacks()) {
        Log(0).stream() << "Callback registration error: OMPT interface disabled" << endl;
        return 0;
    }

    Log(1).stream() << "OMPT callbacks enabled." << endl;

    return 1;
}

} // extern "C"

namespace cali
{
    CaliperService OmptService = { "ompt", { &::omptservice_initialize } };
}
