// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// PthreadService.cpp
// Service for pthreads-based threading runtimes

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"

#include <pthread.h>

#include <gotcha/gotcha.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

using namespace cali;

namespace
{

gotcha_wrappee_handle_t  orig_pthread_create_handle = 0x0;

Attribute id_attr = Attribute::invalid;
Attribute master_attr = Attribute::invalid;

struct wrapper_args {
    void* (*fn)(void*);
    void* arg;
};

// Wrapper for the user-provided thread start function.
// We wrap the original thread start function to create Caliper thread scope
// on the new child thread.
void*
thread_wrapper(void *arg)
{
    uint64_t id = static_cast<uint64_t>(pthread_self());
    Caliper  c;

    c.set(master_attr, Variant(false));
    c.set(id_attr,     Variant(cali_make_variant_from_uint(id)));

    wrapper_args* wrap = static_cast<wrapper_args*>(arg);
    void* ret = (*(wrap->fn))(wrap->arg);

    delete wrap;
    return ret;
}

// Wrapper for pthread_create()
int
cali_pthread_create_wrapper(pthread_t *thread, const pthread_attr_t *attr,
                            void *(*fn)(void*), void* arg)
{
    decltype(&pthread_create) orig_pthread_create =
        reinterpret_cast<decltype(&pthread_create)>(gotcha_get_wrappee(orig_pthread_create_handle));

    return (*orig_pthread_create)(thread, attr, thread_wrapper, new wrapper_args({ fn, arg }));
}

void
post_init_cb(Caliper* c, Channel* channel)
{
    channel->events().subscribe_attribute(c, channel, id_attr);

    static bool is_wrapped = false;

    if (!is_wrapped) {
        static struct gotcha_binding_t pthread_binding[] = {
            { "pthread_create", (void*) cali_pthread_create_wrapper, &orig_pthread_create_handle }
        };

        gotcha_wrap(pthread_binding, sizeof(pthread_binding)/sizeof(struct gotcha_binding_t),
                    "caliper/pthread");

        is_wrapped = true;

        uint64_t id = static_cast<uint64_t>(pthread_self());

        c->set(master_attr, Variant(true));
        c->set(id_attr,     Variant(cali_make_variant_from_uint(id)));
    }
}

// Initialization routine.
void
pthreadservice_initialize(Caliper* c, Channel* chn)
{
    Attribute subscription_attr = c->get_attribute("subscription_event");
    Variant v_true(true);

    id_attr =
        c->create_attribute("pthread.id", CALI_TYPE_UINT,
                            CALI_ATTR_SCOPE_THREAD |
                            CALI_ATTR_UNALIGNED,
                            1, &subscription_attr, &v_true);
    master_attr =
        c->create_attribute("pthread.is_master", CALI_TYPE_BOOL,
                            CALI_ATTR_SCOPE_THREAD |
                            CALI_ATTR_UNALIGNED    |
                            CALI_ATTR_SKIP_EVENTS);

    chn->events().post_init_evt.connect(
        [](Caliper* c, Channel* chn){
            post_init_cb(c, chn);
        });

    Log(1).stream() << chn->name() << ": Registered pthread service" << std::endl;
}

} // namespace [anonymous]

namespace cali
{

CaliperService pthread_service { "pthread", ::pthreadservice_initialize };

}
