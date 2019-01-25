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

#include "SplayTree.hpp"

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <numeric>

#define NUM_TRACKED_ALLOC_ATTRS 2

using namespace cali;
using namespace util;

namespace cali
{

extern Attribute class_memoryaddress_attr;

}

namespace
{

struct AllocInfo {
    uint64_t                 start_addr;
    uint64_t                 total_size;
    Variant                  v_uid;
    Variant                  v_size;
    size_t                   elem_size;
    size_t                   num_elems;
    std::vector<cali::Node*> memattr_label_nodes;
    std::vector<size_t>      dimensions;
    std::string              label;
    
    size_t index_1D(uint64_t addr) const {
        return (addr - start_addr) / elem_size;
    }
};

/// Three-way predicate to tell if given address is within AllocInfo's address range, less, or higher
class ContainsAddress {
    uint64_t address;

public:

    ContainsAddress(uint64_t a)
        : address(a) { }

    inline int operator ()(const AllocInfo& info) const {
        if (address < info.start_addr)
            return -1;
        else if (address >= info.start_addr && address < info.start_addr + info.total_size)
            return 0;
        else 
            return +1;
    }
};

/// Three-way predicate to tell if given address is AllocInfo's start address, less, or higher
class HasStartAddress {
    uint64_t address;

public:

    HasStartAddress(uint64_t a)
        : address(a) { }

    inline int operator ()(const AllocInfo& info) const {
        if (address < info.start_addr)
            return -1;
        else if (address == info.start_addr)
            return 0;
        else 
            return +1;
    }
};

struct AllocInfoCmp {
    int operator ()(const AllocInfo& lhs, const AllocInfo& rhs) const {
        if (lhs.start_addr < rhs.start_addr)
            return -1;
        else if (lhs.start_addr == rhs.start_addr)
            return 0;
        return 1;
    }
};

class AllocService
{
    static const ConfigSet::Entry s_configdata[];
    
    bool g_resolve_addresses        { false };
    bool g_track_allocations        { true  };
    bool g_record_active_mem        { false };

// DataTracker attributes
    Attribute mem_alloc_attr        { Attribute::invalid };
    Attribute mem_free_attr         { Attribute::invalid };
    Attribute alloc_uid_attr        { Attribute::invalid };
    Attribute alloc_addr_attr       { Attribute::invalid };
    Attribute alloc_elem_size_attr  { Attribute::invalid };
    Attribute alloc_num_elems_attr  { Attribute::invalid };
    Attribute alloc_total_size_attr { Attribute::invalid };
    Attribute active_mem_attr       { Attribute::invalid };

// Derived attributes for class.memoryaddress attributes 
    struct alloc_attrs {
        Attribute memoryaddress_attr;
        Attribute alloc_label_attr;
        Attribute alloc_uid_attr;
        Attribute alloc_index_attr;
    };

    std::atomic<unsigned long> g_alloc_uid       { 0 };
    std::vector<alloc_attrs>   g_memoryaddress_attrs;

    cali::Node                 g_alloc_root_node { CALI_INV_ID, CALI_INV_ID, Variant() };

    util::SplayTree<AllocInfo, AllocInfoCmp> g_tree;
    std::mutex                 g_tree_lock;

    uint64_t                   g_active_mem      { 0 };

    unsigned long              g_current_tracked { 0 };
    unsigned long              g_max_tracked     { 0 };
    unsigned long              g_total_tracked   { 0 };
    unsigned                   g_failed_untrack  { 0 };

    void track_mem_snapshot(Caliper*    c,
                            Channel* chn,
                            const Attribute& alloc_or_free_attr, 
                            const Variant&   v_label, 
                            const Variant&   v_size, 
                            const Variant&   v_uid) {
        Attribute attr[] = {
            alloc_or_free_attr,
            alloc_total_size_attr,
            alloc_uid_attr
        };
        Variant   data[] = {
            v_label,
            v_size,
            v_uid
        };

        SnapshotRecord::FixedSnapshotRecord<3> snapshot_data;
        SnapshotRecord trigger_info(snapshot_data);

        c->make_record(3, attr, data, trigger_info, &g_alloc_root_node);
        c->push_snapshot(chn, CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);    
    }

    void track_mem_cb(Caliper* c, Channel* chn, const void* ptr, const char* label, size_t elem_size, size_t ndims, const size_t* dims) {
        size_t total_size =
            std::accumulate(dims, dims+ndims, elem_size, std::multiplies<size_t>());

        AllocInfo info;

        info.start_addr = reinterpret_cast<uint64_t>(ptr);
        info.total_size = total_size;
        info.v_uid      = Variant(cali_make_variant_from_uint(++g_alloc_uid));
        info.v_size     = Variant(cali_make_variant_from_uint(total_size));
        info.elem_size  = elem_size;
        info.num_elems  = total_size / elem_size;

        info.memattr_label_nodes.resize(g_memoryaddress_attrs.size());
        info.dimensions.assign(dims, dims+ndims);

        info.label      = label;

        Variant v_label(CALI_TYPE_STRING, label, strlen(label));
        
        // make label nodes for each memory address attribute
        for (std::vector<cali::Node*>::size_type i = 0; i < g_memoryaddress_attrs.size(); ++i)
            info.memattr_label_nodes[i] =
                c->make_tree_entry(g_memoryaddress_attrs[i].alloc_label_attr, v_label, &g_alloc_root_node);

        {
            std::lock_guard<std::mutex>
                g(g_tree_lock);
        
            g_tree.insert(info);
        
            g_max_tracked  = std::max(++g_current_tracked, g_max_tracked);
            g_active_mem  += total_size;
            ++g_total_tracked;
        }

        if (g_track_allocations)
            track_mem_snapshot(c, chn, mem_alloc_attr, v_label, info.v_size, info.v_uid);
    }

    void untrack_mem_cb(Caliper* c, Channel* chn, const void* ptr) {    
        std::lock_guard<std::mutex>
            g(g_tree_lock);
    
        auto tree_node = g_tree.find(HasStartAddress(reinterpret_cast<uint64_t>(ptr)));
    
        if (tree_node) {
            size_t size = (*tree_node).total_size;

            if (g_track_allocations)
                track_mem_snapshot(c, chn, mem_free_attr,
                                   Variant(CALI_TYPE_STRING, (*tree_node).label.c_str(), (*tree_node).label.size()),
                                   (*tree_node).v_size,
                                   (*tree_node).v_uid);

            g_active_mem -= (*tree_node).total_size;
            g_tree.remove(tree_node);
        
            --g_current_tracked;
        } else {
            ++g_failed_untrack;
        }
    }

    void resolve_addresses(Caliper* c, const SnapshotRecord* trigger_info, SnapshotRecord* snapshot) {
        for (std::vector<alloc_attrs>::size_type i = 0; i < g_memoryaddress_attrs.size(); ++i) {
            Entry e = trigger_info->get(g_memoryaddress_attrs[i].memoryaddress_attr);

            if (e.is_empty())
                continue;

            uint64_t addr = e.value().to_uint();

            cali_id_t   attr[2] = {
                g_memoryaddress_attrs[i].alloc_uid_attr.id(),
                g_memoryaddress_attrs[i].alloc_index_attr.id()
            };
            Variant     data[2];
            cali::Node* label_node = nullptr;

            {
                std::lock_guard<std::mutex>
                    g(g_tree_lock);

                auto tree_node = g_tree.find(ContainsAddress(addr));

                if (!tree_node)
                    continue;

                data[0] = (*tree_node).v_uid;
                data[1] = cali_make_variant_from_uint((*tree_node).index_1D(addr));

                if (i < (*tree_node).memattr_label_nodes.size())
                    label_node = (*tree_node).memattr_label_nodes[i];
            }

            snapshot->append(2, attr, data);

            if (label_node)
                snapshot->append(label_node);
        }
    }

    void snapshot_cb(Caliper* c, Channel* chn, int scope, const SnapshotRecord* trigger_info, SnapshotRecord *snapshot) {
        // Record currently active amount of allocated memory
        if (g_record_active_mem)
            snapshot->append(active_mem_attr.id(), Variant(cali_make_variant_from_uint(g_active_mem)));

        if (g_resolve_addresses)
            resolve_addresses(c, trigger_info, snapshot);
    }

    void make_address_attributes(Caliper* c, const Attribute& attr) {
        struct alloc_attrs attrs = {
            attr,
            c->create_attribute("alloc.label#" + attr.name(), CALI_TYPE_STRING,
                                CALI_ATTR_DEFAULT),
            c->create_attribute("alloc.uid#"   + attr.name(), CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE),
            c->create_attribute("alloc.index#" + attr.name(), CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE)
        };

        g_memoryaddress_attrs.push_back(attrs);    
    }

    void create_attr_cb(Caliper* c, const Attribute& attr) {
        // Note: this isn't threadsafe!
        if (attr.get(class_memoryaddress_attr).to_bool() == true)
            make_address_attributes(c, attr);
    }

    void post_init_cb(Caliper *c, Channel* chn) {
        if (!g_resolve_addresses)
            return;

        std::vector<Attribute> address_attrs =
            c->find_attributes_with(c->get_attribute("class.memoryaddress"));

        for (auto attr : address_attrs)
            make_address_attributes(c, attr);

        chn->events().create_attr_evt.connect(
            [this](Caliper* c, Channel* chn, const Attribute& attr){
                this->create_attr_cb(c, attr);
            });
    }

    void finish_cb(Caliper* c, Channel* chn) {
        Log(1).stream() << chn->name() << ": alloc: "
                        << g_total_tracked  << " memory allocations tracked (max "
                        << g_max_tracked    << " simultaneous), "
                        << g_failed_untrack << " untrack lookups failed."
                        << std::endl;
    }

    AllocService(Caliper* c, Channel* chn)
        {
            struct attr_info_t {
                const char*    name;
                cali_attr_type type;
                int            prop;
                Attribute*     attr;
            } attr_info[] = {
                { "mem.alloc",        CALI_TYPE_STRING, CALI_ATTR_DEFAULT,
                  &mem_alloc_attr
                },
                { "mem.free" ,        CALI_TYPE_STRING, CALI_ATTR_DEFAULT,
                  &mem_free_attr
                },
                { "mem.active" ,      CALI_TYPE_UINT,   CALI_ATTR_ASVALUE,
                  &active_mem_attr
                },
                { "alloc.uid",        CALI_TYPE_UINT,   CALI_ATTR_ASVALUE,
                  &alloc_uid_attr
                },
                { "alloc.address",    CALI_TYPE_ADDR,   CALI_ATTR_ASVALUE,
                  &alloc_addr_attr
                },
                { "alloc.elem_size",  CALI_TYPE_UINT,   CALI_ATTR_DEFAULT,
                  &alloc_elem_size_attr
                },
                { "alloc.num_elems",  CALI_TYPE_UINT,   CALI_ATTR_DEFAULT,
                  &alloc_num_elems_attr
                },
                { "alloc.total_size", CALI_TYPE_UINT,   CALI_ATTR_DEFAULT,
                  &alloc_total_size_attr
                },
                { 0, CALI_TYPE_INV, CALI_ATTR_DEFAULT, nullptr }
            };
        
            for (attr_info_t *p = attr_info; p->name; ++p)
                *(p->attr) = c->create_attribute(p->name, p->type, p->prop);
    
            ConfigSet config = chn->config().init("alloc", s_configdata);
    
            g_resolve_addresses = config.get("resolve_addresses").to_bool();
            g_track_allocations = config.get("track_allocations").to_bool();
            g_record_active_mem = config.get("record_active_mem").to_bool();
        }

public:
    
    static void allocservice_initialize(Caliper* c, Channel* chn) {
        AllocService* instance = new AllocService(c, chn);

        chn->events().track_mem_evt.connect(
            [instance](Caliper* c, Channel* chn, const void* ptr, const char* label, size_t elem_size, size_t ndims, const size_t* dims){
                instance->track_mem_cb(c, chn, ptr, label, elem_size, ndims, dims);
            });
        chn->events().untrack_mem_evt.connect(
            [instance](Caliper* c, Channel* chn, const void* ptr){
                instance->untrack_mem_cb(c, chn, ptr);
            });

        if (instance->g_resolve_addresses || instance->g_record_active_mem)
            chn->events().snapshot.connect(
                [instance](Caliper* c, Channel* chn, int scope, const SnapshotRecord* info, SnapshotRecord* snapshot){
                    instance->snapshot_cb(c, chn, scope, info, snapshot);
                });
       
        chn->events().post_init_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->post_init_cb(c, chn);
            });
        chn->events().finish_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->finish_cb(c, chn);
                delete instance;
            });

        Log(1).stream() << chn->name() << ": Registered alloc service" << std::endl;
    }
};

const ConfigSet::Entry AllocService::s_configdata[] = {
    { "resolve_addresses", CALI_TYPE_BOOL, "false",
      "Whether to resolve memory addresses in snapshots.",
      "Whether to resolve memory addresses in snapshots.\n"
    },
    { "track_allocations", CALI_TYPE_BOOL, "true",
      "Whether to record snapshots for annotated memory allocations.",
      "Whether to record snapshots for annotated memory allocations.\n"
    },
    { "record_active_mem", CALI_TYPE_BOOL, "false",
      "Record the active allocated memory at each snapshot.",
      "Record the active allocated memory at each snapshot."
    },
    
    ConfigSet::Terminator
};

} // namespace [anonymous]


namespace cali
{

CaliperService alloc_service { "alloc", ::AllocService::allocservice_initialize };

}
