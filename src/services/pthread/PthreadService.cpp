///@file  PthreadService.cpp
///@brief Service for pthreads-based threading runtimes

#include "../CaliperService.h"

#include <Caliper.h>

#include <Log.h>

#include <cstdlib>
#include <pthread.h>


using namespace cali;
using namespace std;

namespace
{

pthread_key_t thread_env_key;

void save_environment(cali_id_t env)
{
    cali_id_t* ptr = static_cast<cali_id_t*>(malloc(sizeof(cali_id_t)));
    *ptr = env;
    pthread_setspecific(thread_env_key, ptr);
}

cali_id_t
get_thread_environment()
{
    cali_id_t  env = 0;
    cali_id_t* ptr = static_cast<cali_id_t*>(pthread_getspecific(thread_env_key));

    if (ptr) 
        env = *ptr;
    else {
        env = Caliper::instance()->create_environment();
        save_environment(env);
    } 

    return env;
}

/// Initialization routine. 
/// Create the thread-local storage key, register the environment callback, and 
/// map current (initialization) thread to default environment
void pthreadservice_initialize(Caliper* c)
{
    pthread_key_create(&thread_env_key, &std::free);
    save_environment(c->default_environment(CALI_SCOPE_THREAD));

    c->set_environment_callback(CALI_SCOPE_THREAD, &get_thread_environment);

    Log(1).stream() << "Registered pthread service" << endl;
}

} // namespace [anonymous]

namespace cali
{
    CaliperService PthreadService { "pthread", { &::pthreadservice_initialize } };
}
