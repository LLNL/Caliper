// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "../util/ChannelList.hpp"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"

#include <gotcha/gotcha.h>

#include <algorithm>
#include <vector>

#include <errno.h>

using namespace cali;
using util::ChannelList;

namespace
{

gotcha_wrappee_handle_t orig_malloc_handle  = 0x0;
gotcha_wrappee_handle_t orig_calloc_handle  = 0x0;
gotcha_wrappee_handle_t orig_realloc_handle = 0x0;
gotcha_wrappee_handle_t orig_free_handle    = 0x0;

void* cali_malloc_wrapper(size_t size);
void* cali_calloc_wrapper(size_t num, size_t size);
void* cali_realloc_wrapper(void *ptr, size_t size);
void cali_free_wrapper(void *ptr);

const char *malloc_str = "malloc";
const char *calloc_str = "calloc";
const char *realloc_str = "realloc";
const char *free_str = "free";

bool bindings_are_active = false;

struct gotcha_binding_t alloc_bindings[] = {
    { malloc_str,   (void*) cali_malloc_wrapper,    &orig_malloc_handle  },
    { calloc_str,   (void*) cali_calloc_wrapper,    &orig_calloc_handle  },
    { realloc_str,  (void*) cali_realloc_wrapper,   &orig_realloc_handle },
    { free_str,     (void*) cali_free_wrapper,      &orig_free_handle    }
};

ChannelList* sysalloc_channels = nullptr;


void* cali_malloc_wrapper(size_t size)
{
    decltype(&malloc) orig_malloc =
        reinterpret_cast<decltype(&malloc)>(gotcha_get_wrappee(orig_malloc_handle));

    void *ret = (*orig_malloc)(size);

    int saved_errno = errno;

    for (ChannelList* p = sysalloc_channels; p; p = p->next) {
        Caliper c = Caliper::sigsafe_instance(); // prevent reentry

        if (c && p->channel->is_active())
            c.memory_region_begin(p->channel, ret, "malloc", 1, 1, &size);
    }

    errno = saved_errno;

    return ret;
}

void* cali_calloc_wrapper(size_t num, size_t size)
{
    decltype(&calloc) orig_calloc =
        reinterpret_cast<decltype(&calloc)>(gotcha_get_wrappee(orig_calloc_handle));

    void *ret = (*orig_calloc)(num, size);

    int saved_errno = errno;

    for (ChannelList* p = sysalloc_channels; p; p = p->next) {
        Caliper c = Caliper::sigsafe_instance(); // prevent reentry

        if (c && p->channel->is_active())
            c.memory_region_begin(p->channel, ret, "calloc", size, 1, &num);
    }

    errno = saved_errno;

    return ret;
}

void* cali_realloc_wrapper(void *ptr, size_t size)
{
    decltype(&realloc) orig_realloc =
        reinterpret_cast<decltype(&realloc)>(gotcha_get_wrappee(orig_realloc_handle));

    for (ChannelList* p = sysalloc_channels; p; p = p->next) {
        Caliper c = Caliper::sigsafe_instance();

        if (c && p->channel->is_active())
            c.memory_region_end(p->channel, ptr);
    }

    void *ret = (*orig_realloc)(ptr, size);

    int saved_errno = errno;

    for (ChannelList* p = sysalloc_channels; p; p = p->next) {
        Caliper c = Caliper::sigsafe_instance();

        if (c && p->channel->is_active())
            c.memory_region_begin(p->channel, ret, "realloc", 1, 1, &size);
    }

    errno = saved_errno;

    return ret;
}

void cali_free_wrapper(void *ptr)
{
    decltype(&free) orig_free =
        reinterpret_cast<decltype(&free)>(gotcha_get_wrappee(orig_free_handle));

    for (ChannelList* p = sysalloc_channels; p; p = p->next) {
        Caliper c = Caliper::sigsafe_instance();

        if (c && p->channel->is_active())
            c.memory_region_end(p->channel, ptr);
    }

    (*orig_free)(ptr);
}


void init_alloc_hooks() {
    Log(1).stream() << "sysalloc: Initializing system alloc hooks" << std::endl;

    gotcha_wrap(alloc_bindings,
                sizeof(alloc_bindings)/sizeof(struct gotcha_binding_t),
                "caliper/sysalloc");

    bindings_are_active = true;
}

#if 0
void clear_alloc_hooks()
{
    if (!bindings_are_active)
        return;

    Log(1).stream() << "sysalloc: Removing system alloc hooks" << std::endl;

    gotcha_wrappee_handle_t dummy = 0x0;

    struct gotcha_binding_t orig_bindings[] = {
        { malloc_str,  gotcha_get_wrappee(orig_malloc_handle),  &dummy },
        { calloc_str,  gotcha_get_wrappee(orig_calloc_handle),  &dummy },
        { realloc_str, gotcha_get_wrappee(orig_realloc_handle), &dummy },
        { free_str,    gotcha_get_wrappee(orig_free_handle),    &dummy }
    };

    gotcha_wrap(orig_bindings,
                sizeof(orig_bindings)/sizeof(struct gotcha_binding_t),
                "caliper/sysalloc");

    bindings_are_active = false;
}
#endif

void sysalloc_initialize(Caliper* c, Channel* chn) {
    chn->events().post_init_evt.connect(
        [](Caliper* c, Channel* chn){
            if (!bindings_are_active)
                init_alloc_hooks();

            ChannelList::add(&sysalloc_channels, chn);
        });

    chn->events().finish_evt.connect(
        [](Caliper* c, Channel* chn){
            Log(2).stream() << chn->name() << ": Removing sysalloc hooks" << std::endl;
            ChannelList::remove(&sysalloc_channels, chn);
        });

    Log(1).stream() << chn->name() << ": Registered sysalloc service" << std::endl;
}

} // namespace [anonymous]


namespace cali
{

CaliperService sysalloc_service { "sysalloc", ::sysalloc_initialize };

}
