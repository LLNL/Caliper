// Copyright (c) 2018, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by Alfredo Gimenez, gimenez1@llnl.gov.
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

#include "caliper/CaliperService.h"

#include "../util/ChannelList.hpp"

#include "caliper/Caliper.h"
#include "caliper/Annotation.h"

#include "caliper/common/Log.h"

#include <gotcha/gotcha.h>

#include <algorithm>
#include <vector>
#include <iostream>

#include <errno.h>

using namespace cali;
using util::ChannelList;


enum cudaMemcpyKind {
  cudaMemcpyHostToHost,
  cudaMemcpyHostToDevice,
  cudaMemcpyDeviceToHost,
  cudaMemcpyDeviceToDevice
};

namespace
{

gotcha_wrappee_handle_t orig_malloc_handle  =  nullptr;
gotcha_wrappee_handle_t orig_free_handle    =  nullptr;
gotcha_wrappee_handle_t orig_memcpy_handle    =  nullptr;

int cali_malloc_wrapper(void** ptr, size_t size);
int cali_free_wrapper(void *ptr);
int cali_memcpy_wrapper(void*, void*, size_t, /** cudaMemcpyHostToDevice */ int);

const char *malloc_str = "cudaMalloc";
const char *free_str = "cudaFree";
const char *memcpy_str = "cudaMemcpy";

bool bindings_are_active = false;

struct gotcha_binding_t alloc_bindings[] = {
    { malloc_str,   (void*) cali_malloc_wrapper,    &orig_malloc_handle  },
    { memcpy_str,   (void*) cali_memcpy_wrapper,    &orig_memcpy_handle  },
    { free_str,     (void*) cali_free_wrapper,      &orig_free_handle    }
};

ChannelList* sysalloc_channels = nullptr;



int cali_malloc_wrapper(void** ptr,const size_t size)
{
    decltype(&cali_malloc_wrapper) orig_malloc =
        reinterpret_cast<decltype(&cali_malloc_wrapper)>(gotcha_get_wrappee(orig_malloc_handle));
    
    auto ret = (*orig_malloc)(ptr, size);

    Caliper c = Caliper::sigsafe_instance(); // prevent reentry

    if (c)
        for (ChannelList* p = sysalloc_channels; p; p = p->next)
            if (p->channel->is_active())
                c.memory_region_begin(p->channel, *ptr, "cudaMalloc", 1, 1, &size);

    return ret;
}

int cali_free_wrapper(void *ptr)
{
    decltype(&cali_free_wrapper) orig_free =
        reinterpret_cast<decltype(&cali_free_wrapper)>(gotcha_get_wrappee(orig_free_handle));

    Caliper c = Caliper::sigsafe_instance();

    if (c)
        for (ChannelList* p = sysalloc_channels; p; p = p->next)
            if (p->channel->is_active())
                c.memory_region_end(p->channel, ptr);

    auto ret = (*orig_free)(ptr);

    return ret;

}

void record_copy(void* dst, void* src,  size_t size, int direction){
    static cali::Annotation trigger_annot("trigger_for_event",CALI_ATTR_HIDDEN);
    static cali::Annotation dst_annot("copy#destination",CALI_ATTR_SKIP_EVENTS);
    static cali::Annotation src_annot("copy#source",CALI_ATTR_SKIP_EVENTS);
    static cali::Annotation size_annot("copy#size",CALI_ATTR_SKIP_EVENTS);

    dst_annot.begin(cali::Variant(CALI_TYPE_ADDR,&dst,sizeof(void*)));
    src_annot.begin(cali::Variant(CALI_TYPE_ADDR,&src,sizeof(void*)));
    size_annot.begin((int)size);
    trigger_annot.begin();
    trigger_annot.end();
    size_annot.end();
    src_annot.end();
    dst_annot.end();
}

int cali_memcpy_wrapper(void* dst, void* src, size_t size, int direction){
    decltype(&cali_memcpy_wrapper) orig_memcpy =
        reinterpret_cast<decltype(&cali_memcpy_wrapper)>(gotcha_get_wrappee(orig_memcpy_handle));
    
    Caliper c = Caliper::sigsafe_instance();

    if (c){
        record_copy(dst,src,size,direction);
    }

    auto ret =(*orig_memcpy)(dst, src, size, direction);

    return ret;
}


void init_alloc_hooks() {
    Log(1).stream() << "sysalloc: Initializing system alloc hooks" << std::endl;

    gotcha_wrap(alloc_bindings,
                sizeof(alloc_bindings)/sizeof(struct gotcha_binding_t),
                "caliper/sysalloc");

    bindings_are_active = true;
}

void clear_alloc_hooks()
{
    if (!bindings_are_active)
        return;
    
    Log(1).stream() << "sysalloc: Removing system alloc hooks" << std::endl;

    gotcha_wrappee_handle_t dummy = 0x0;

    struct gotcha_binding_t orig_bindings[] = {
        { malloc_str,  gotcha_get_wrappee(orig_malloc_handle),  &dummy },
        { memcpy_str, gotcha_get_wrappee(orig_memcpy_handle), &dummy },
        { free_str,    gotcha_get_wrappee(orig_free_handle),    &dummy }
    };

    gotcha_wrap(orig_bindings,
                sizeof(orig_bindings)/sizeof(struct gotcha_binding_t),
                "caliper/sysalloc");

    bindings_are_active = false;
}

void cudawrap_initialize(Caliper* c, Channel* chn) {    
    chn->events().post_init_evt.connect(
        [](Caliper* c, Channel* chn){
            if (!bindings_are_active)
                init_alloc_hooks();

            ChannelList::add(&sysalloc_channels, chn);
        });
    
    chn->events().finish_evt.connect(
        [](Caliper* c, Channel* chn){
            ChannelList::remove(&sysalloc_channels, chn);

            if (sysalloc_channels == nullptr)
                clear_alloc_hooks();
        });

    Log(1).stream() << chn->name() << ": Registered sysalloc service" << std::endl;
}

} // namespace [anonymous]


namespace cali
{

CaliperService cudawrap_service { "cudawrap", ::cudawrap_initialize };

}
