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

#include <atomic>
#include <cstring>
#include <mutex>

#include <gotcha/gotcha.h>

using namespace cali;

namespace
{
    __thread std::mutex thread_mutex;
    bool record_all_allocs { false };
    bool track_all_allocs { false };

    ConfigSet config;

    const ConfigSet::Entry s_configdata[] = {
            { "record_all", CALI_TYPE_BOOL, "false",
              "Record all allocations made by malloc/calloc/realloc.",
              "Record all allocations made by malloc/calloc/realloc. May incur high overhead for code with frequent allocations."
            },
            { "track_all", CALI_TYPE_BOOL, "false",
              "Track all allocations made by malloc/calloc/realloc.",
              "Track all allocations made by malloc/calloc/realloc. May incur high overhead for code with frequent allocations."
            },
            ConfigSet::Terminator
    };

    struct alloc_attrs {
        Attribute memoryaddress_attr;
        Attribute alloc_label_attr;
        Attribute alloc_index_attr;
    };

    std::vector<alloc_attrs> memoryaddress_attrs;

    std::atomic<uint64_t> alloc_count { 0 };

    Attribute class_memoryaddress_attr = Attribute::invalid;

    Attribute alloc_fn_attr = Attribute::invalid;
    Attribute alloc_prev_addr_attr = Attribute::invalid;
    Attribute alloc_index_attr = Attribute::invalid;

    Attribute alloc_label_attr = Attribute::invalid;
    Attribute alloc_addr_attr = Attribute::invalid;
    Attribute alloc_elem_size_attr = Attribute::invalid;
    Attribute alloc_num_elems_attr = Attribute::invalid;
    Attribute alloc_total_size_attr = Attribute::invalid;

#define NUM_TRACKED_ALLOC_ATTRS 2

#define NUM_MALLOC_ATTRS 4
#define NUM_CALLOC_ATTRS 6
#define NUM_REALLOC_ATTRS 5
#define NUM_FREE_ATTRS 3

    void* (*orig_malloc)(size_t size) = nullptr;
    void* (*orig_calloc)(size_t num, size_t size) = nullptr;
    void* (*orig_realloc)(void* ptr, size_t size) = nullptr;
    void  (*orig_free)(void* ptr) = nullptr;

    void* cali_malloc_wrapper(size_t size);
    void* cali_calloc_wrapper(size_t num, size_t size);
    void* cali_realloc_wrapper(void *ptr, size_t size);
    void cali_free_wrapper(void *ptr);

    const char *malloc_str = "malloc";
    const char *calloc_str = "calloc";
    const char *realloc_str = "realloc";
    const char *free_str = "free";

    struct gotcha_binding_t alloc_bindings[] = {
            { malloc_str,   (void*) cali_malloc_wrapper,    &orig_malloc },
            { calloc_str,   (void*) cali_calloc_wrapper,    &orig_calloc },
            { realloc_str,  (void*) cali_realloc_wrapper,   &orig_realloc },
            { free_str,     (void*) cali_free_wrapper,      &orig_free }
    };

    Variant malloc_str_variant = Variant(CALI_TYPE_STRING, malloc_str, strlen(malloc_str));
    Variant calloc_str_variant = Variant(CALI_TYPE_STRING, calloc_str, strlen(calloc_str));
    Variant realloc_str_variant = Variant(CALI_TYPE_STRING, realloc_str, strlen(realloc_str));
    Variant free_str_variant = Variant(CALI_TYPE_STRING, free_str, strlen(free_str));

    static cali_id_t malloc_attributes[NUM_MALLOC_ATTRS] = {CALI_INV_ID};
    static cali_id_t calloc_attributes[NUM_CALLOC_ATTRS] = {CALI_INV_ID};
    static cali_id_t realloc_attributes[NUM_REALLOC_ATTRS] = {CALI_INV_ID};
    static cali_id_t free_attributes[NUM_FREE_ATTRS] = {CALI_INV_ID};

    void* cali_malloc_wrapper(size_t size)
    {
        void *ret = (*orig_malloc)(size);

        std::unique_lock<std::mutex> thread_lock(thread_mutex, std::try_to_lock);
        if(thread_lock.owns_lock()) {

            Caliper c = Caliper::sigsafe_instance();

            if (c) {
                if (track_all_allocs) {
                    size_t dims[] = {size};
                    DataTracker::g_alloc_tracker.add_allocation(std::to_string(alloc_count), 
                            (uint64_t)ret, (size_t)1, dims, (size_t)1);
                }

                if (record_all_allocs) {
                    Variant data[NUM_MALLOC_ATTRS];

                    data[0] = malloc_str_variant;
                    data[1] = (uint64_t)alloc_count;
                    data[2] = static_cast<uint64_t>(size);
                    data[3] = (uint64_t)ret;

                    SnapshotRecord trigger_info(NUM_MALLOC_ATTRS, malloc_attributes, data);
                    c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);
                }

                alloc_count++;
            }
        }

        return ret;
    }

    void* cali_calloc_wrapper(size_t num, size_t size)
    {
        void *ret = (*orig_calloc)(num, size);

        std::unique_lock<std::mutex> thread_lock(thread_mutex, std::try_to_lock);
        if(thread_lock.owns_lock()) {

            Caliper c = Caliper::sigsafe_instance();

            if (c) {
                if (track_all_allocs) {
                    size_t dims[] = {num};
                    DataTracker::g_alloc_tracker.add_allocation(std::to_string(alloc_count), (uint64_t)ret, size, dims, 1);
                }

                if (record_all_allocs) {
                    Variant data[NUM_CALLOC_ATTRS];

                    data[0] = calloc_str_variant;
                    data[1] = (uint64_t)alloc_count;
                    data[2] = static_cast<uint64_t>(num);
                    data[3] = static_cast<uint64_t>(size);
                    data[4] = (uint64_t)ret;
                    data[5] = data[1]*data[2]; // num*size

                    SnapshotRecord trigger_info(NUM_CALLOC_ATTRS, calloc_attributes, data);
                    c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);
                }

                alloc_count++;
            }
        }

        return ret;
    }

    void* cali_realloc_wrapper(void *ptr, size_t size)
    {
        void *ret = (*orig_realloc)(ptr, size);

        std::unique_lock<std::mutex> thread_lock(thread_mutex, std::try_to_lock);
        if(thread_lock.owns_lock()) {

            Caliper c = Caliper::sigsafe_instance();

            if (c) {
                bool removed = DataTracker::g_alloc_tracker.remove_allocation((uint64_t)ptr);

                if (removed || track_all_allocs) {
                    size_t dims[] = {size};
                    DataTracker::g_alloc_tracker.add_allocation(std::to_string(alloc_count), (uint64_t)ret, (size_t)1, dims, 1);
                }

                if (record_all_allocs) {
                    Variant data[NUM_REALLOC_ATTRS];

                    data[0] = realloc_str_variant;
                    data[1] = (uint64_t)alloc_count;
                    data[2] = ((uint64_t)ptr);
                    data[3] = static_cast<uint64_t>(size);
                    data[4] = (uint64_t)ret;

                    SnapshotRecord trigger_info(NUM_REALLOC_ATTRS, realloc_attributes, data);
                    c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);
                }

                alloc_count++;
            }
        }

        return ret;
    }

    void cali_free_wrapper(void *ptr)
    {
        (*orig_free)(ptr);

        std::unique_lock<std::mutex> thread_lock(thread_mutex, std::try_to_lock);
        if(thread_lock.owns_lock()) {

            Caliper c = Caliper::sigsafe_instance();

            if (c) {
                // Always check the allocation tracker for frees
                DataTracker::g_alloc_tracker.remove_allocation((uint64_t)ptr);

                if (record_all_allocs) {
                    Variant data[NUM_FREE_ATTRS];

                    data[0] = free_str_variant;
                    data[1] = (uint64_t)alloc_count;
                    data[2] = ((uint64_t)ptr);

                    SnapshotRecord trigger_info(NUM_FREE_ATTRS, free_attributes, data);
                    c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);
                }

                alloc_count++;
            }
        }
    }

    void init_alloc_hooks(Caliper *c) {
        gotcha_wrap(alloc_bindings, 
                    sizeof(alloc_bindings)/sizeof(struct gotcha_binding_t), 
                    "Caliper AllocService Wrap");
    }

    static void snapshot_cb(Caliper* c, int scope, const SnapshotRecord* trigger_info, SnapshotRecord *snapshot) {
        for (auto attrs : memoryaddress_attrs) {

            Entry e = snapshot->get(attrs.memoryaddress_attr);

            if (!e.is_empty()) {

                uint64_t memory_address = e.value().to_uint();
                cali::DataTracker::Allocation *alloc = DataTracker::g_alloc_tracker.find_allocation_containing(memory_address);

                if (alloc) {
                    const size_t index = alloc->index_1D(memory_address);

                    // TODO: for n-dimensional indices:
                    // TODO:    support tuples? or custom ways to print CALI_TYPE_USR data?
                    //const size_t *index = alloc->index_ND(memory_address);

                    Attribute attr[NUM_TRACKED_ALLOC_ATTRS] = {
                        attrs.alloc_label_attr,
                        attrs.alloc_index_attr
                    };
                    Variant data[NUM_TRACKED_ALLOC_ATTRS] = {
                        Variant(CALI_TYPE_STRING, alloc->m_label.data(), alloc->m_label.size()),
                        Variant(index),
                    };

                    c->make_entrylist(NUM_TRACKED_ALLOC_ATTRS, attr, data, *snapshot);
                }
            }
        }
    }

    static void post_init_cb(Caliper *c) {

        class_memoryaddress_attr = c->get_attribute("class.memoryaddress");

        alloc_label_attr = c->get_attribute("alloc.label");
        alloc_addr_attr = c->get_attribute("alloc.address");
        alloc_elem_size_attr = c->get_attribute("alloc.elem_size");
        alloc_num_elems_attr = c->get_attribute("alloc.num_elems");
        alloc_total_size_attr = c->get_attribute("alloc.total_size");

        // Additional hook-specific attributes
        alloc_fn_attr = c->create_attribute("alloc.fn", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
        alloc_prev_addr_attr = c->create_attribute("alloc.prev_address", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);

        // Set per-hook attributes
        malloc_attributes[0] = alloc_fn_attr.id();
        malloc_attributes[1] = alloc_label_attr.id();
        malloc_attributes[2] = alloc_total_size_attr.id();
        malloc_attributes[3] = alloc_addr_attr.id();

        calloc_attributes[0] = alloc_fn_attr.id();
        calloc_attributes[1] = alloc_label_attr.id();
        calloc_attributes[2] = alloc_num_elems_attr.id();
        calloc_attributes[3] = alloc_elem_size_attr.id();
        calloc_attributes[4] = alloc_addr_attr.id();
        calloc_attributes[5] = alloc_total_size_attr.id();

        realloc_attributes[0] = alloc_fn_attr.id();
        realloc_attributes[1] = alloc_label_attr.id();
        realloc_attributes[2] = alloc_prev_addr_attr.id();
        realloc_attributes[3] = alloc_total_size_attr.id();
        realloc_attributes[4] = alloc_addr_attr.id();

        free_attributes[0] = alloc_fn_attr.id();
        free_attributes[1] = alloc_label_attr.id();
        free_attributes[2] = alloc_prev_addr_attr.id();

        std::vector<Attribute> memory_address_attrs = c->find_attributes_with(class_memoryaddress_attr);

        for (auto attr : memory_address_attrs) {
            struct alloc_attrs attrs = {
                attr,
                c->create_attribute("alloc.label#" + attr.name(), CALI_TYPE_STRING, CALI_ATTR_DEFAULT),
                c->create_attribute("alloc.index#" + attr.name(), CALI_TYPE_UINT, CALI_ATTR_DEFAULT)
            };
            memoryaddress_attrs.push_back(attrs);
        }

    }

    static void pre_flush_cb(Caliper* c, const SnapshotRecord*) {
        thread_mutex.lock();
    }

    // Initialization routine.
    void
    allocservice_initialize(Caliper* c)
    {
        config = RuntimeConfig::init("alloc", s_configdata);

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
