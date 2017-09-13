// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
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

///\file  MallocService.cpp
///\brief Service for hooking memory allocation calls

#include "../CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"

#include <cstdlib>

#include <gotcha/gotcha.h>

using namespace cali;

namespace
{
    /**
     * malloc
     */

#define NUM_MALLOC_ATTRS 3

    void* (*orig_malloc)(size_t size) = NULL;

    Attribute malloc_id_attr = Attribute::invalid;
    Attribute malloc_size_attr = Attribute::invalid;
    Attribute malloc_addr_attr = Attribute::invalid;

    static cali_id_t malloc_attributes[NUM_MALLOC_ATTRS] = {CALI_INV_ID};

    uint64_t malloc_count = 0;

    void*
    cali_malloc_wrapper(size_t size)
    {
        Caliper c = Caliper::sigsafe_instance();

        // Run malloc
        void *ret = (*orig_malloc)(size);

        if (c) {
            // Create and push snapshot
            Variant data[NUM_MALLOC_ATTRS];

            data[0] = malloc_count++;
            data[1] = static_cast<uint64_t>(size);
            data[2] = (uint64_t)ret;

            // TODO: get timestamp
            // TODO: get callpath

            SnapshotRecord trigger_info(NUM_MALLOC_ATTRS, malloc_attributes, data);
            c.push_snapshot(CALI_SCOPE_PROCESS, &trigger_info);
        }

        return ret;
    }

    /**
     * calloc
     */

    void* (*orig_calloc)(size_t num, size_t size) = NULL;

#define NUM_CALLOC_ATTRS 4

    Attribute calloc_id_attr = Attribute::invalid;
    Attribute calloc_num_attr = Attribute::invalid;
    Attribute calloc_size_attr = Attribute::invalid;
    Attribute calloc_addr_attr = Attribute::invalid;

    static cali_id_t calloc_attributes[NUM_CALLOC_ATTRS] = {CALI_INV_ID};

    uint64_t calloc_count = 0;

    void*
    cali_calloc_wrapper(size_t num, size_t size)
    {
        Caliper c = Caliper::sigsafe_instance();

        // Run malloc
        void *ret = (*orig_calloc)(num, size);

        if (c) {
            // Create and push snapshot
            Variant data[NUM_CALLOC_ATTRS];

            data[0] = calloc_count++;
            data[1] = static_cast<uint64_t>(num);
            data[2] = static_cast<uint64_t>(size);
            data[3] = (uint64_t)ret;

            // TODO: get timestamp
            // TODO: get callpath

            SnapshotRecord trigger_info(NUM_CALLOC_ATTRS, calloc_attributes, data);
            c.push_snapshot(CALI_SCOPE_PROCESS, &trigger_info);
        }

        return ret;
    }

    /**
     * realloc
     */

    void* (*orig_realloc)(void* ptr, size_t size) = NULL;

#define NUM_REALLOC_ATTRS 4

    Attribute realloc_id_attr = Attribute::invalid;
    Attribute realloc_ptr_attr = Attribute::invalid;
    Attribute realloc_size_attr = Attribute::invalid;
    Attribute realloc_addr_attr = Attribute::invalid;

    static cali_id_t realloc_attributes[NUM_REALLOC_ATTRS] = {CALI_INV_ID};

    uint64_t realloc_count = 0;

    void*
    cali_realloc_wrapper(void *ptr, size_t size)
    {
        Caliper c = Caliper::sigsafe_instance();

        // Run malloc
        void *ret = (*orig_realloc)(ptr, size);

        if (c) {
            // Create and push snapshot
            Variant data[NUM_REALLOC_ATTRS];

            data[0] = realloc_count++;
            data[1] = ((uint64_t)ptr);
            data[2] = static_cast<uint64_t>(size);
            data[3] = (uint64_t)ret;

            // TODO: get timestamp
            // TODO: get callpath

            SnapshotRecord trigger_info(NUM_REALLOC_ATTRS, realloc_attributes, data);
            c.push_snapshot(CALI_SCOPE_PROCESS, &trigger_info);
        }

        return ret;
    }

    /**
     * free
     */

    void (*orig_free)(void* ptr) = NULL;

#define NUM_FREE_ATTRS 2

    Attribute free_id_attr = Attribute::invalid;
    Attribute free_addr_attr = Attribute::invalid;

    static cali_id_t free_attributes[NUM_FREE_ATTRS] = {CALI_INV_ID};

    uint64_t free_count = 0;

    void
    cali_free_wrapper(void *ptr)
    {
        Caliper c = Caliper::sigsafe_instance();

        // Run original
        (*orig_free)(ptr);

        if (c) {
            // Create and push snapshot
            Variant data[NUM_FREE_ATTRS];

            data[0] = free_count++;
            data[1] = ((uint64_t)ptr);

            // TODO: get timestamp
            // TODO: get callpath

            SnapshotRecord trigger_info(NUM_FREE_ATTRS, free_attributes, data);
            c.push_snapshot(CALI_SCOPE_PROCESS, &trigger_info);
        }
    }

// Initialization routine.
    void
    mallocservice_initialize(Caliper* c)
    {
        malloc_id_attr = c->create_attribute("malloc.id", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        malloc_size_attr = c->create_attribute("malloc.size", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        malloc_addr_attr = c->create_attribute("malloc.address", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);

        malloc_attributes[0] = malloc_id_attr.id();
        malloc_attributes[1] = malloc_size_attr.id();
        malloc_attributes[2] = malloc_addr_attr.id();

        calloc_id_attr = c->create_attribute("calloc.id", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        calloc_size_attr = c->create_attribute("calloc.num", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        calloc_size_attr = c->create_attribute("calloc.size", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        calloc_addr_attr = c->create_attribute("calloc.address", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);

        calloc_attributes[0] = calloc_id_attr.id();
        calloc_attributes[1] = calloc_num_attr.id();
        calloc_attributes[2] = calloc_size_attr.id();
        calloc_attributes[3] = calloc_addr_attr.id();

        realloc_id_attr = c->create_attribute("realloc.id", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        realloc_ptr_attr = c->create_attribute("realloc.ptr", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        realloc_size_attr = c->create_attribute("realloc.size", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        realloc_addr_attr = c->create_attribute("realloc.address", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);

        realloc_attributes[0] = realloc_id_attr.id();
        realloc_attributes[1] = realloc_ptr_attr.id();
        realloc_attributes[2] = realloc_size_attr.id();
        realloc_attributes[3] = realloc_addr_attr.id();

        free_id_attr = c->create_attribute("free.id", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        free_addr_attr = c->create_attribute("free.address", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);

        free_attributes[0] = free_id_attr.id();
        free_attributes[1] = free_addr_attr.id();

        struct gotcha_binding_t malloc_binding[] = {
                { "malloc", (void*) cali_malloc_wrapper, &orig_malloc },
                { "calloc", (void*) cali_calloc_wrapper, &orig_calloc },
                { "realloc", (void*) cali_realloc_wrapper, &orig_realloc },
                { "free", (void*) cali_free_wrapper, &orig_free }
        };

        gotcha_wrap(malloc_binding, sizeof(malloc_binding)/sizeof(struct gotcha_binding_t), "Caliper");

        Log(1).stream() << "Registered malloc service" << std::endl;
    }

} // namespace [anonymous]

namespace cali
{
    CaliperService malloc_service { "malloc", ::mallocservice_initialize };
}
