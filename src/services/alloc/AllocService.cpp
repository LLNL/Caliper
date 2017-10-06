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

///\file  AllocService.cpp
///\brief Service for hooking memory allocation calls

#include "../CaliperService.h"

#include "caliper/DataTracker.h"
#include "caliper/AllocTracker.h"
#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <gotcha/gotcha.h>

using namespace cali;

namespace
{
    bool hook_enabled { false };

    ConfigSet config;

    const ConfigSet::Entry s_configdata[] = {
            { "hook_malloc", CALI_TYPE_BOOL, "false",
              "Hook calls to malloc/calloc/realloc/free.",
              "Hook calls to malloc/calloc/realloc/free. May incur high overhead for code with frequent allocations."
            },
            ConfigSet::Terminator
    };

    // FIXME: alloc count is possibly(?) not unique in threaded scenarios
    uint64_t alloc_count { 0 };
    Attribute alloc_id_attr = Attribute::invalid;

    /**
     * malloc
     */

#define NUM_MALLOC_ATTRS 3

    void* (*orig_malloc)(size_t size) = NULL;

    Attribute malloc_id_attr = Attribute::invalid;
    Attribute malloc_size_attr = Attribute::invalid;
    Attribute malloc_addr_attr = Attribute::invalid;

    static cali_id_t malloc_attributes[NUM_MALLOC_ATTRS] = {CALI_INV_ID};

    void*
    cali_malloc_wrapper(size_t size)
    {
        Caliper c = Caliper::sigsafe_instance();

        void *ret = (*orig_malloc)(size);

        if (hook_enabled) {
            hook_enabled = false;
            if (c) {
                DataTracker::g_alloc_tracker.add_allocation(std::to_string(alloc_count), (uint64_t)ret, (size_t)1, {size});

                Variant data[NUM_MALLOC_ATTRS];

                data[0] = alloc_count;
                data[1] = static_cast<uint64_t>(size);
                data[2] = (uint64_t)ret;

                SnapshotRecord trigger_info(NUM_MALLOC_ATTRS, malloc_attributes, data);
                c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);

                alloc_count++;
            }
            hook_enabled = true;
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

    void*
    cali_calloc_wrapper(size_t num, size_t size)
    {
        Caliper c = Caliper::sigsafe_instance();

        void *ret = (*orig_calloc)(num, size);

        if (hook_enabled) {
            hook_enabled = false;
            if (c) {
                DataTracker::g_alloc_tracker.add_allocation(std::to_string(alloc_count), (uint64_t)ret, size, {num});

                Variant data[NUM_CALLOC_ATTRS];

                data[0] = alloc_count;
                data[1] = static_cast<uint64_t>(num);
                data[2] = static_cast<uint64_t>(size);
                data[3] = (uint64_t)ret;

                SnapshotRecord trigger_info(NUM_CALLOC_ATTRS, calloc_attributes, data);
                c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);

                alloc_count++;
            }
            hook_enabled = true;
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

    void*
    cali_realloc_wrapper(void *ptr, size_t size)
    {
        Caliper c = Caliper::sigsafe_instance();

        void *ret = (*orig_realloc)(ptr, size);

        if (hook_enabled) {
            hook_enabled = false;
            if (c) {
                DataTracker::g_alloc_tracker.remove_allocation((uint64_t)ptr);
                DataTracker::g_alloc_tracker.add_allocation(std::to_string(alloc_count), (uint64_t)ret, (size_t)1, {size});

                Variant data[NUM_REALLOC_ATTRS];

                data[0] = alloc_count;
                data[1] = ((uint64_t)ptr);
                data[2] = static_cast<uint64_t>(size);
                data[3] = (uint64_t)ret;

                SnapshotRecord trigger_info(NUM_REALLOC_ATTRS, realloc_attributes, data);
                c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);

                alloc_count++;
            }
            hook_enabled = true;
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

    void
    cali_free_wrapper(void *ptr)
    {
        Caliper c = Caliper::sigsafe_instance();

        (*orig_free)(ptr);

        if (hook_enabled) {
            hook_enabled = false;
            if (c) {
                DataTracker::g_alloc_tracker.remove_allocation((uint64_t)ptr);

                Variant data[NUM_FREE_ATTRS];

                data[0] = alloc_count;
                data[1] = ((uint64_t)ptr);

                SnapshotRecord trigger_info(NUM_FREE_ATTRS, free_attributes, data);
                c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);

                alloc_count++;
            }
            hook_enabled = true;
        }
    }

    void init_alloc_hooks(Caliper *c) {
        malloc_id_attr = c->create_attribute("alloc.malloc.label", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        malloc_size_attr = c->create_attribute("alloc.malloc.size", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        malloc_addr_attr = c->create_attribute("alloc.malloc.address", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);

        malloc_attributes[0] = malloc_id_attr.id();
        malloc_attributes[1] = malloc_size_attr.id();
        malloc_attributes[2] = malloc_addr_attr.id();

        calloc_id_attr = c->create_attribute("alloc.calloc.label", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        calloc_size_attr = c->create_attribute("alloc.calloc.num", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        calloc_size_attr = c->create_attribute("alloc.calloc.size", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        calloc_addr_attr = c->create_attribute("alloc.calloc.address", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);

        calloc_attributes[0] = calloc_id_attr.id();
        calloc_attributes[1] = calloc_num_attr.id();
        calloc_attributes[2] = calloc_size_attr.id();
        calloc_attributes[3] = calloc_addr_attr.id();

        realloc_id_attr = c->create_attribute("alloc.realloc.label", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        realloc_ptr_attr = c->create_attribute("alloc.realloc.ptr", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        realloc_size_attr = c->create_attribute("alloc.realloc.size", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        realloc_addr_attr = c->create_attribute("alloc.realloc.address", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);

        realloc_attributes[0] = realloc_id_attr.id();
        realloc_attributes[1] = realloc_ptr_attr.id();
        realloc_attributes[2] = realloc_size_attr.id();
        realloc_attributes[3] = realloc_addr_attr.id();

        free_id_attr = c->create_attribute("alloc.free.label", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        free_addr_attr = c->create_attribute("alloc.free.address", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);

        free_attributes[0] = free_id_attr.id();
        free_attributes[1] = free_addr_attr.id();

        struct gotcha_binding_t alloc_bindings[] = {
                { "malloc", (void*) cali_malloc_wrapper, &orig_malloc },
                { "calloc", (void*) cali_calloc_wrapper, &orig_calloc },
                { "realloc", (void*) cali_realloc_wrapper, &orig_realloc },
                { "free", (void*) cali_free_wrapper, &orig_free }
        };

        gotcha_wrap(alloc_bindings, sizeof(alloc_bindings)/sizeof(struct gotcha_binding_t), "Caliper");
    }

    std::vector<Attribute> memory_address_attrs;

    static void snapshot_cb(Caliper* c, int scope, const SnapshotRecord* trigger_info, SnapshotRecord *snapshot) {
        bool prev_hook_enabled = hook_enabled;
        hook_enabled = false;

        if (!memory_address_attrs.empty()) {

            // TODO: do this for every memoryaddress attribute
            Attribute attr = memory_address_attrs.front();
            Entry e = snapshot->get(attr);

            if (!e.is_empty()) {

                uint64_t memory_address = e.value().to_uint();
                Allocation *alloc = DataTracker::g_alloc_tracker.find_allocation_containing(memory_address);

                if (alloc) {
                    Attribute attr[1] = { alloc_id_attr };
                    Variant data[1] = { Variant(CALI_TYPE_STRING, alloc->m_label.data(), alloc->m_label.size()) };

                    c->make_entrylist(1, attr, data, *snapshot);
                }
            }
        }

        hook_enabled = prev_hook_enabled;
    }

    static void post_init_cb(Caliper *c) {

        // TODO: make an alloc.id per memoryaddress attribute, e.g. alloc.label#libpfm.address
        alloc_id_attr = c->create_attribute("alloc.label", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

        memory_address_attrs = c->find_attributes_with(c->get_attribute("class.memoryaddress"));
    }

    static void pre_flush_cb(Caliper* c, const SnapshotRecord*) {
        // Disable the hook for Caliper flush
        hook_enabled = false;
    }

    // Initialization routine.
    void
    allocservice_initialize(Caliper* c)
    {
        config = RuntimeConfig::init("callpath", s_configdata);

        hook_enabled = config.get("use_name").to_bool();

        c->events().post_init_evt.connect(post_init_cb);
        c->events().snapshot.connect(snapshot_cb);
        c->events().pre_flush_evt.connect(pre_flush_cb);

        init_alloc_hooks(c);

        Log(1).stream() << "Registered alloc service" << std::endl;
    }

} // namespace [anonymous]

namespace cali
{
    CaliperService alloc_service { "alloc", ::allocservice_initialize };
}
