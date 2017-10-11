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
    bool hook_enabled { true };
    bool record_all_allocs { false };
    bool track_all_allocs { false };

    ConfigSet config;

    const ConfigSet::Entry s_configdata[] = {
            { "record_all_allocs", CALI_TYPE_BOOL, "false",
              "Record all allocations made by malloc/calloc/realloc.",
              "Record all allocations made by malloc/calloc/realloc. May incur high overhead for code with frequent allocations."
            },
            { "track_all_allocs", CALI_TYPE_BOOL, "false",
              "Track all allocations made by malloc/calloc/realloc.",
              "Track all allocations made by malloc/calloc/realloc. May incur high overhead for code with frequent allocations."
            },
            ConfigSet::Terminator
    };

    // FIXME: alloc count is possibly(?) not unique in threaded scenarios
    uint64_t alloc_count { 0 };

    Attribute alloc_fn_attr = Attribute::invalid;
    Attribute alloc_label_attr = Attribute::invalid;
    Attribute alloc_size_attr = Attribute::invalid;
    Attribute alloc_prev_addr_attr = Attribute::invalid;
    Attribute alloc_addr_attr = Attribute::invalid;
    Attribute alloc_num_elems_attr = Attribute::invalid;
    Attribute alloc_elem_size_attr = Attribute::invalid;
    Attribute alloc_index_attr = Attribute::invalid;

#define NUM_FREE_ATTRS 2
#define NUM_TRACKED_ALLOC_ATTRS 2
#define NUM_MALLOC_ATTRS 3
#define NUM_CALLOC_ATTRS 5
#define NUM_REALLOC_ATTRS 4

    void* (*orig_malloc)(size_t size) = nullptr;
    void* (*orig_calloc)(size_t num, size_t size) = nullptr;
    void* (*orig_realloc)(void* ptr, size_t size) = nullptr;
    void  (*orig_free)(void* ptr) = nullptr;

    static cali_id_t malloc_attributes[NUM_MALLOC_ATTRS] = {CALI_INV_ID};
    static cali_id_t calloc_attributes[NUM_CALLOC_ATTRS] = {CALI_INV_ID};
    static cali_id_t realloc_attributes[NUM_REALLOC_ATTRS] = {CALI_INV_ID};
    static cali_id_t free_attributes[NUM_FREE_ATTRS] = {CALI_INV_ID};

    void*
    cali_malloc_wrapper(size_t size)
    {
        Caliper c = Caliper::sigsafe_instance();

        void *ret = (*orig_malloc)(size);

        if (hook_enabled) {
            hook_enabled = false;
            if (c) {
                if (track_all_allocs) {
                    DataTracker::g_alloc_tracker.add_allocation(std::to_string(alloc_count), (uint64_t)ret, (size_t)1, {size});
                }

                if (record_all_allocs) {
                    Variant data[NUM_MALLOC_ATTRS];

                    data[0] = alloc_count;
                    data[1] = static_cast<uint64_t>(size);
                    data[2] = (uint64_t)ret;

                    SnapshotRecord trigger_info(NUM_MALLOC_ATTRS, malloc_attributes, data);
                    c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);
                }

                alloc_count++;
            }
            hook_enabled = true;
        }

        return ret;
    }

    void*
    cali_calloc_wrapper(size_t num, size_t size)
    {
        Caliper c = Caliper::sigsafe_instance();

        void *ret = (*orig_calloc)(num, size);

        if (hook_enabled) {
            hook_enabled = false;
            if (c) {
                if (track_all_allocs) {
                    DataTracker::g_alloc_tracker.add_allocation(std::to_string(alloc_count), (uint64_t)ret, size, {num});
                }

                if (record_all_allocs) {
                    Variant data[NUM_CALLOC_ATTRS];

                    data[0] = alloc_count;
                    data[1] = static_cast<uint64_t>(num);
                    data[2] = static_cast<uint64_t>(size);
                    data[3] = (uint64_t)ret;
                    data[4] = data[1]*data[2]; // num*size

                    SnapshotRecord trigger_info(NUM_CALLOC_ATTRS, calloc_attributes, data);
                    c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);
                }

                alloc_count++;
            }
            hook_enabled = true;
        }

        return ret;
    }

    void*
    cali_realloc_wrapper(void *ptr, size_t size)
    {
        Caliper c = Caliper::sigsafe_instance();

        void *ret = (*orig_realloc)(ptr, size);

        if (hook_enabled) {
            hook_enabled = false;
            if (c) {
                bool removed = DataTracker::g_alloc_tracker.remove_allocation((uint64_t)ptr);

                if (removed || track_all_allocs) {
                    DataTracker::g_alloc_tracker.add_allocation(std::to_string(alloc_count), (uint64_t)ret, (size_t)1, {size});
                }

                if (record_all_allocs) {
                    Variant data[NUM_REALLOC_ATTRS];

                    data[0] = alloc_count;
                    data[1] = ((uint64_t)ptr);
                    data[2] = static_cast<uint64_t>(size);
                    data[3] = (uint64_t)ret;

                    SnapshotRecord trigger_info(NUM_REALLOC_ATTRS, realloc_attributes, data);
                    c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);
                }

                alloc_count++;
            }
            hook_enabled = true;
        }

        return ret;
    }

    void
    cali_free_wrapper(void *ptr)
    {
        Caliper c = Caliper::sigsafe_instance();

        (*orig_free)(ptr);

        if (hook_enabled) {
            hook_enabled = false;
            if (c) {
                // Always check the allocation tracker for frees
                DataTracker::g_alloc_tracker.remove_allocation((uint64_t)ptr);

                if (record_all_allocs) {
                    Variant data[NUM_FREE_ATTRS];

                    data[0] = alloc_count;
                    data[1] = ((uint64_t)ptr);

                    SnapshotRecord trigger_info(NUM_FREE_ATTRS, free_attributes, data);
                    c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);
                }

                alloc_count++;
            }
            hook_enabled = true;
        }
    }

    void init_alloc_hooks(Caliper *c) {

        // Global attributes for 
        alloc_fn_attr = c->create_attribute("alloc.fn", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
        alloc_label_attr = c->create_attribute("alloc.label", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
        alloc_size_attr = c->create_attribute("alloc.size", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        alloc_prev_addr_attr = c->create_attribute("alloc.prev_address", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        alloc_addr_attr = c->create_attribute("alloc.address", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        alloc_num_elems_attr = c->create_attribute("alloc.num_elems", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);
        alloc_elem_size_attr = c->create_attribute("alloc.elem_size", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);

        malloc_attributes[0] = alloc_label_attr.id();
        malloc_attributes[1] = alloc_size_attr.id();
        malloc_attributes[2] = alloc_addr_attr.id();

        calloc_attributes[0] = alloc_label_attr.id();
        calloc_attributes[1] = alloc_num_elems_attr.id();
        calloc_attributes[2] = alloc_elem_size_attr.id();
        calloc_attributes[3] = alloc_addr_attr.id();
        calloc_attributes[4] = alloc_size_attr.id();

        realloc_attributes[0] = alloc_label_attr.id();
        realloc_attributes[1] = alloc_prev_addr_attr.id();
        realloc_attributes[2] = alloc_size_attr.id();
        realloc_attributes[3] = alloc_addr_attr.id();

        free_attributes[0] = alloc_label_attr.id();
        free_attributes[1] = alloc_prev_addr_attr.id();

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
                cali::DataTracker::Allocation *alloc = DataTracker::g_alloc_tracker.find_allocation_containing(memory_address);

                if (alloc) {
                    const size_t index = alloc->index_1D(memory_address);

                    // TODO: for n-dimensional indices:
                    // TODO:    support tuples? or custom ways to print CALI_TYPE_USR data?
                    //const size_t *index = alloc->index_ND(memory_address);

                    Attribute attr[NUM_TRACKED_ALLOC_ATTRS] = {
                            alloc_label_attr,
                            alloc_index_attr
                    };
                    Variant data[NUM_TRACKED_ALLOC_ATTRS] = {
                            Variant(CALI_TYPE_STRING, alloc->m_label.data(), alloc->m_label.size()),
                            Variant(index),
                    };

                    c->make_entrylist(NUM_TRACKED_ALLOC_ATTRS, attr, data, *snapshot);
                }
            }
        }

        hook_enabled = prev_hook_enabled;
    }

    static void post_init_cb(Caliper *c) {

        // TODO: make an alloc.id per memoryaddress attribute, e.g. alloc.label#libpfm.address
        alloc_label_attr = c->create_attribute("alloc.label", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
        alloc_index_attr = c->create_attribute("alloc.index", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);

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

        record_all_allocs = config.get("record_all").to_bool();
        track_all_allocs = config.get("track_all").to_bool();

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
