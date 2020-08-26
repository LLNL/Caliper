// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Service for hooking memory allocation calls

#include "SplayTree.hpp"

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"

#include <algorithm>
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

#define MAX_ADDRESS_ATTRIBUTES 4

struct AllocInfo {
    uint64_t                 start_addr;
    uint64_t                 total_size;
    Variant                  v_uid;
    size_t                   elem_size;
    size_t                   num_elems;

    cali::Node*              alloc_label_node;
    cali::Node*              free_label_node;
    cali::Node*              addr_label_nodes[MAX_ADDRESS_ATTRIBUTES];

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
    bool g_record_highwatermark     { false };

// DataTracker attributes
    Attribute mem_alloc_attr        { Attribute::invalid };
    Attribute mem_free_attr         { Attribute::invalid };
    Attribute alloc_uid_attr        { Attribute::invalid };
    Attribute alloc_addr_attr       { Attribute::invalid };
    Attribute alloc_elem_size_attr  { Attribute::invalid };
    Attribute alloc_num_elems_attr  { Attribute::invalid };
    Attribute alloc_total_size_attr { Attribute::invalid };
    Attribute active_mem_attr       { Attribute::invalid };

    Attribute region_hwm_attr       { Attribute::invalid };

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
    uint64_t                   g_hwm             { 0 };
    uint64_t                   g_region_hwm      { 0 };
    std::mutex                 g_hwm_lock;

    unsigned long              g_current_tracked { 0 };
    unsigned long              g_max_tracked     { 0 };
    unsigned long              g_total_tracked   { 0 };
    unsigned                   g_failed_untrack  { 0 };

    void track_mem_snapshot(Caliper* c,
                            Channel* chn,
                            cali::Node*    label_node,
                            const Variant& v_size,
                            const Variant& v_uid,
                            const Variant& v_addr) {
        cali_id_t attr[] = {
            alloc_total_size_attr.id(),
            alloc_uid_attr.id(),
            alloc_addr_attr.id()
        };
        Variant   data[] = {
            v_size,
            v_uid,
            v_addr
        };

        SnapshotRecord trigger_info(1, &label_node, 3, attr, data);
        c->push_snapshot(chn, &trigger_info);
    }

    void track_mem_cb(Caliper* c, Channel* chn, const void* ptr, const char* label, size_t elem_size, size_t ndims, const size_t* dims,
                      size_t nextra, const Attribute* extra_attrs, const Variant* extra_vals) {
        size_t total_size =
            std::accumulate(dims, dims+ndims, elem_size, std::multiplies<size_t>());

        AllocInfo info;

        info.start_addr = reinterpret_cast<uint64_t>(ptr);
        info.total_size = total_size;
        info.v_uid      = Variant(cali_make_variant_from_uint(++g_alloc_uid));
        info.elem_size  = elem_size;
        info.num_elems  = total_size / elem_size;

        Variant v_label(label);

        Node* root_node = &g_alloc_root_node;

        for (size_t i = 0; i < nextra; ++i)
            root_node = c->make_tree_entry(extra_attrs[i], extra_vals[i], root_node);

        info.alloc_label_node =
            c->make_tree_entry(mem_alloc_attr, v_label, root_node);
        info.free_label_node  =
            c->make_tree_entry(mem_free_attr,  v_label, root_node);

        std::fill_n(info.addr_label_nodes, MAX_ADDRESS_ATTRIBUTES, nullptr);

        for (int i = 0; i < std::min<int>(g_memoryaddress_attrs.size(), MAX_ADDRESS_ATTRIBUTES); ++i) {
            info.addr_label_nodes[i] =
                c->make_tree_entry(g_memoryaddress_attrs[i].alloc_label_attr, v_label, root_node);
        }

        if (g_track_allocations)
            track_mem_snapshot(c, chn, info.alloc_label_node,
                               Variant(static_cast<int>(total_size)),
                               info.v_uid,
                               Variant(CALI_TYPE_ADDR, &ptr, sizeof(void*)));

        {
            std::lock_guard<std::mutex>
                g(g_hwm_lock);

            g_active_mem  += total_size;
            g_hwm          = std::max(g_hwm, g_active_mem);
            g_region_hwm   = std::max(g_region_hwm, g_active_mem);
        }

        {
            std::lock_guard<std::mutex>
                g(g_tree_lock);

            g_tree.insert(info);

            g_max_tracked  = std::max(++g_current_tracked, g_max_tracked);
            ++g_total_tracked;
        }
    }

    void untrack_mem_cb(Caliper* c, Channel* chn, const void* ptr) {
        AllocInfo info;

        {
            std::lock_guard<std::mutex>
                g(g_tree_lock);

            auto tree_node = g_tree.find(HasStartAddress(reinterpret_cast<uint64_t>(ptr)));

            if (!tree_node) {
                ++g_failed_untrack;
                return;
            }

            info = *tree_node;
            g_tree.remove(tree_node);

            --g_current_tracked;
        }

        if (g_track_allocations)
            track_mem_snapshot(c, chn, info.free_label_node,
                               Variant(-static_cast<int>(info.total_size)),
                               info.v_uid,
                               Variant(CALI_TYPE_ADDR, &ptr, sizeof(void*)));

        {
            std::lock_guard<std::mutex>
                g(g_hwm_lock);

            g_active_mem -= info.total_size;
        }
    }

    void resolve_addresses(Caliper* c, const SnapshotRecord* trigger_info, SnapshotRecord* snapshot) {
        for (int i = 0; i < std::min<int>(g_memoryaddress_attrs.size(), MAX_ADDRESS_ATTRIBUTES); ++i) {
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

                label_node = (*tree_node).addr_label_nodes[i];
            }

            snapshot->append(2, attr, data);

            if (label_node)
                snapshot->append(label_node);
        }
    }

    void record_highwatermark(Caliper* c, Channel* chn, SnapshotRecord* rec) {
        uint64_t hwm = 0;

        {
            std::lock_guard<std::mutex>
                g(g_hwm_lock);

            hwm          = g_region_hwm;
            g_region_hwm = g_active_mem;
        }

        rec->append(region_hwm_attr.id(), Variant(hwm));
    }

    void snapshot_cb(Caliper* c, Channel* chn, int scope, const SnapshotRecord* trigger_info, SnapshotRecord *snapshot) {
        // Record currently active amount of allocated memory
        if (g_record_active_mem)
            snapshot->append(active_mem_attr.id(), Variant(cali_make_variant_from_uint(g_active_mem)));

        if (g_resolve_addresses && trigger_info != nullptr)
            resolve_addresses(c, trigger_info, snapshot);

        if (g_record_highwatermark)
            record_highwatermark(c, chn, snapshot);
    }

    void make_address_attributes(Caliper* c, const Attribute& attr) {
        struct alloc_attrs attrs = {
            attr,
            c->create_attribute("alloc.label#" + attr.name(), CALI_TYPE_STRING,
                                CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS),
            c->create_attribute("alloc.uid#"   + attr.name(), CALI_TYPE_UINT,
                                CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS),
            c->create_attribute("alloc.index#" + attr.name(), CALI_TYPE_UINT,
                                CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS)
        };

        if (g_memoryaddress_attrs.size() >= MAX_ADDRESS_ATTRIBUTES)
            Log(1).stream() << "alloc: Can't perform lookup for more than "
                            << MAX_ADDRESS_ATTRIBUTES
                            << " attributes. Skipping "
                            << attr.name() << std::endl;
        else
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
            Attribute class_aggr_attr =
                c->get_attribute("class.aggregatable");
            Variant   v_true(true);

            struct attr_info_t {
                const char*    name;
                cali_attr_type type;
                int            prop;
                int            meta_count;
                Attribute*     meta_attr;
                Variant*       meta_vals;
                Attribute*     attr;
            } attr_info[] = {
                { "mem.alloc",        CALI_TYPE_STRING,  CALI_ATTR_SCOPE_THREAD,
                  0, nullptr, nullptr,
                  &mem_alloc_attr
                },
                { "mem.free" ,        CALI_TYPE_STRING,  CALI_ATTR_SCOPE_THREAD,
                  0, nullptr, nullptr,
                  &mem_free_attr
                },
                { "mem.active" ,      CALI_TYPE_UINT,    CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE,
                  0, nullptr, nullptr,
                  &active_mem_attr
                },
                { "alloc.uid",        CALI_TYPE_UINT,    CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE,
                  0, nullptr, nullptr,
                  &alloc_uid_attr
                },
                { "alloc.address",    CALI_TYPE_ADDR,    CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE,
                  0, nullptr, nullptr,
                  &alloc_addr_attr
                },
                { "alloc.elem_size",  CALI_TYPE_UINT,    CALI_ATTR_SCOPE_THREAD,
                  0, nullptr, nullptr,
                  &alloc_elem_size_attr
                },
                { "alloc.num_elems",  CALI_TYPE_UINT,    CALI_ATTR_SCOPE_THREAD,
                  0, nullptr, nullptr,
                  &alloc_num_elems_attr
                },
                { "alloc.total_size", CALI_TYPE_INT,     CALI_ATTR_SCOPE_THREAD  | CALI_ATTR_ASVALUE,
                  1, &class_aggr_attr, &v_true,
                  &alloc_total_size_attr
                },
                { "alloc.region.highwatermark", CALI_TYPE_UINT, CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE,
                  1, &class_aggr_attr, &v_true,
                  &region_hwm_attr
                },
                { 0, CALI_TYPE_INV, CALI_ATTR_DEFAULT, 0, nullptr, nullptr, nullptr }
            };

            for (attr_info_t *p = attr_info; p->name; ++p)
                *(p->attr) = c->create_attribute(p->name, p->type, p->prop | CALI_ATTR_SKIP_EVENTS, p->meta_count, p->meta_attr, p->meta_vals);

            ConfigSet config = chn->config().init("alloc", s_configdata);

            g_resolve_addresses    = config.get("resolve_addresses").to_bool();
            g_track_allocations    = config.get("track_allocations").to_bool();
            g_record_active_mem    = config.get("record_active_mem").to_bool();
            g_record_highwatermark = config.get("record_highwatermark").to_bool();
        }

public:

    static void allocservice_initialize(Caliper* c, Channel* chn) {
        AllocService* instance = new AllocService(c, chn);

        chn->events().track_mem_evt.connect(
            [instance](Caliper* c, Channel* chn, const void* ptr, const char* label, size_t elem_size, size_t ndims, const size_t* dims, size_t n, const Attribute* attrs, const Variant* vals){
                instance->track_mem_cb(c, chn, ptr, label, elem_size, ndims, dims, n, attrs, vals);
            });
        chn->events().untrack_mem_evt.connect(
            [instance](Caliper* c, Channel* chn, const void* ptr){
                instance->untrack_mem_cb(c, chn, ptr);
            });

        if (instance->g_resolve_addresses || instance->g_record_active_mem || instance->g_record_highwatermark)
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
    { "record_highwatermark", CALI_TYPE_BOOL, "false",
      "Record the high water mark of allocated memory at each snapshot.",
      "Record the high water mark of allocated memory at each snapshot."
    },

    ConfigSet::Terminator
};

} // namespace [anonymous]


namespace cali
{

CaliperService alloc_service { "alloc", ::AllocService::allocservice_initialize };

}
