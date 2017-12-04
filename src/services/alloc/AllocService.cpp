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
    thread_local std::mutex thread_mutex;
    bool record_system_allocs { false };
    bool track_ranges { true };
    bool track_system_alloc_ranges { false };
    bool record_active_mem { false };

    std::atomic<unsigned long> alloc_count(0);

    ConfigSet config;

    const ConfigSet::Entry s_configdata[] = {
            { "record_system_allocs", CALI_TYPE_BOOL, "false",
              "Record allocations made by malloc/calloc/realloc.",
              "Record allocations made by malloc/calloc/realloc."
            },
            { "track_system_alloc_ranges", CALI_TYPE_BOOL, "false",
              "Track allocations ranges made by malloc/calloc/realloc.",
              "Track allocations ranges made by malloc/calloc/realloc."
            },
            { "track_ranges", CALI_TYPE_BOOL, "true",
              "Whether to track active memory ranges (not including system allocations, unless used with CALI_ALLOC_TRACK_SYSTEM_ALLOC_RANGES).",
              "Whether to track active memory ranges (not including system allocations, unless used with CALI_ALLOC_TRACK_SYSTEM_ALLOC_RANGES). If enabled, will resolve addresses to their containing allocations."
            },
            { "record_active_mem", CALI_TYPE_BOOL, "false",
              "Record the total allocated memory at each snapshot.",
              "Record the total allocated memory at each snapshot."
            },
            ConfigSet::Terminator
    };

    struct alloc_attrs {
        Attribute memoryaddress_attr;
        Attribute alloc_label_attr;
        Attribute alloc_index_attr;
    };

    std::vector<alloc_attrs> memoryaddress_attrs;

    Attribute class_memoryaddress_attr = Attribute::invalid;

    Attribute active_mem_attr = Attribute::invalid;

#define NUM_TRACKED_ALLOC_ATTRS 2

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
    const char *realloc_free_str = "realloc(free)";
    const char *realloc_alloc_str = "realloc(alloc)";
    const char *free_str = "free";

    struct gotcha_binding_t alloc_bindings[] = {
            { malloc_str,   (void*) cali_malloc_wrapper,    &orig_malloc },
            { calloc_str,   (void*) cali_calloc_wrapper,    &orig_calloc },
            { realloc_str,  (void*) cali_realloc_wrapper,   &orig_realloc },
            { free_str,     (void*) cali_free_wrapper,      &orig_free }
    };

    void* cali_malloc_wrapper(size_t size)
    {
        void *ret = (*orig_malloc)(size);

        std::unique_lock<std::mutex> thread_lock(thread_mutex, std::try_to_lock);
        if(thread_lock.owns_lock()) {

            Caliper c = Caliper::sigsafe_instance();

            if (c) {
                size_t dims[] = {size};
                DataTracker::g_alloc_tracker.add_allocation(std::to_string(alloc_count++), 
                        (uint64_t)ret, (size_t)1, dims, (size_t)1, malloc_str, record_system_allocs, track_system_alloc_ranges);
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
                size_t dims[] = {num};
                DataTracker::g_alloc_tracker.add_allocation(
                        std::to_string(alloc_count++), (uint64_t)ret, size, dims, 1, calloc_str, record_system_allocs, track_system_alloc_ranges);
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
                DataTracker::Allocation removed = 
                    DataTracker::g_alloc_tracker.remove_allocation((uint64_t)ptr, realloc_free_str, record_system_allocs);

                size_t dims[] = {size};
                DataTracker::g_alloc_tracker.add_allocation(
                        std::to_string(alloc_count++), (uint64_t)ret, (size_t)1, dims, 1, realloc_alloc_str, record_system_allocs, track_system_alloc_ranges);
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
                cali::DataTracker::Allocation removed = 
                    DataTracker::g_alloc_tracker.remove_allocation((uint64_t)ptr, free_str, record_system_allocs);
            }
        }
    }

    void init_alloc_hooks(Caliper *c) {
        gotcha_wrap(alloc_bindings, 
                    sizeof(alloc_bindings)/sizeof(struct gotcha_binding_t), 
                    "Caliper AllocService Wrap");
    }

    static void snapshot_cb(Caliper* c, int scope, const SnapshotRecord* trigger_info, SnapshotRecord *snapshot) {

        // Record currently active amount of allocated memory
        if (record_active_mem) {
            Attribute attr[1] = {
                active_mem_attr
            };
            Variant data[1] = {
                Variant(cali::DataTracker::g_alloc_tracker.get_active_bytes()),
            };

            c->make_entrylist(1, attr, data, *snapshot);
        }

        // Attribute any memory addresses with their allocations
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

    static void create_attr_cb(Caliper* c, const Attribute& attr) {
        if (class_memoryaddress_attr == Attribute::invalid) // not initialized yet; skip
            return;

        if (attr.get(class_memoryaddress_attr) == Variant(true)) {
            Log(2).stream() << "alloc: Creating alloc label attributes for attribute " << attr.name() << std::endl;

            struct alloc_attrs attrs = {
                attr,
                c->create_attribute("alloc.label#" + attr.name(), CALI_TYPE_STRING, CALI_ATTR_DEFAULT),
                c->create_attribute("alloc.index#" + attr.name(), CALI_TYPE_UINT, CALI_ATTR_DEFAULT)
            };

            std::lock_guard<std::mutex> thread_lock(thread_mutex);
            memoryaddress_attrs.push_back(attrs);            
        }
    }

    static void post_init_cb(Caliper *c) {

        class_memoryaddress_attr = c->get_attribute("class.memoryaddress");

        active_mem_attr  = c->create_attribute("alloc.active_memory", CALI_TYPE_UINT, CALI_ATTR_ASVALUE);

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

    void allocservice_initialize(Caliper* c)
    {
        config = RuntimeConfig::init("alloc", s_configdata);

        record_system_allocs = config.get("record_system_allocs").to_bool();
        track_system_alloc_ranges = config.get("track_system_alloc_ranges").to_bool();
        track_ranges = config.get("track_ranges").to_bool();
        record_active_mem = config.get("record_active_mem").to_bool();

        cali::DataTracker::g_alloc_tracker.set_track_ranges(track_ranges);

        c->events().create_attr_evt.connect(create_attr_cb);
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
