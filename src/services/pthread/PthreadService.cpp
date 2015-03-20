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

void release_contextbuffer(void* ctx)
{
    Caliper* c = Caliper::instance();

    if (c) {
        c->release_contextbuffer(static_cast<ContextBuffer*>(ctx));
        Log(2).stream() << "Released context buffer" << endl;
    }
}

void save_contextbuffer(ContextBuffer* ctxbuf)
{
    pthread_setspecific(thread_env_key, ctxbuf);
}

ContextBuffer*
get_thread_contextbuffer()
{
    ContextBuffer* ctxbuf = static_cast<ContextBuffer*>(pthread_getspecific(thread_env_key));

    if (!ctxbuf) {
        ctxbuf = Caliper::instance()->create_contextbuffer(CALI_SCOPE_THREAD);
        save_contextbuffer(ctxbuf);
    } 

    return ctxbuf;
}

/// Initialization routine. 
/// Create the thread-local storage key, register the environment callback, and 
/// map current (initialization) thread to default environment
void pthreadservice_initialize(Caliper* c)
{
    pthread_key_create(&thread_env_key, &release_contextbuffer);
    save_contextbuffer(c->default_contextbuffer(CALI_SCOPE_THREAD));

    c->set_contextbuffer_callback(CALI_SCOPE_THREAD, &get_thread_contextbuffer);

    Log(1).stream() << "Registered pthread service" << endl;
}

} // namespace [anonymous]

namespace cali
{
    CaliperService PthreadService { "pthread", { &::pthreadservice_initialize } };
}
