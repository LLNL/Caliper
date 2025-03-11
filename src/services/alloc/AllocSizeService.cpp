// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Service for hooking memory allocation calls

#include "caliper/CaliperService.h"
#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <numeric>
#include <unordered_map>

using namespace cali;
using namespace util;

namespace
{

class AllocSizeService
{
    struct AllocInfo {
        uint64_t bytes;
        cali::Node* path;
    };

    struct RegionInfo {
        uint64_t current_bytes;
        uint64_t total_bytes;
        uint64_t hwm;
        uint64_t count;
        cali::Node* path;
    };

    Attribute alloc_hwm_attr;
    Attribute alloc_count_attr;
    Attribute avg_alloc_size_attr;

    std::unordered_map<uint64_t, AllocInfo>  g_alloc_map;
    std::mutex                               g_alloc_map_lock;
    std::unordered_map<uint64_t, RegionInfo> g_region_map;
    std::mutex                               g_region_map_lock;

    uint64_t   g_active_mem { 0 };
    uint64_t   g_hwm { 0 };
    uint64_t   g_region_hwm { 0 };
    std::mutex g_hwm_lock;

    unsigned g_current_tracked { 0 };
    unsigned g_max_tracked { 0 };
    unsigned g_total_tracked { 0 };
    unsigned g_failed_untrack { 0 };

    void track_mem_cb(
        Caliper*         c,
        Channel*         chn,
        const void*      ptr,
        const char*      label,
        size_t           elem_size,
        size_t           ndims,
        const size_t*    dims,
        size_t           nextra,
        const Attribute* extra_attrs,
        const Variant*   extra_vals
    )
    {
        uint64_t    size = std::accumulate(dims, dims + ndims, elem_size, std::multiplies<size_t>());
        uint64_t    addr = reinterpret_cast<uint64_t>(ptr);
        cali::Node* path = c->get_path_node().node();

        if (!path)
            return;

        {
            AllocInfo info { size, path };
            std::lock_guard<std::mutex> g(g_alloc_map_lock);
            g_alloc_map[addr] = info;

            g_max_tracked = std::max(++g_current_tracked, g_max_tracked);
            ++g_total_tracked;
        }

        {
            std::lock_guard<std::mutex> g(g_region_map_lock);
            auto it = g_region_map.find(path->id());
            if (it == g_region_map.end()) {
                RegionInfo info { size, size, size, 1ull, path };
                g_region_map[path->id()] = info;
            } else {
                it->second.current_bytes += size;
                it->second.total_bytes += size;
                it->second.hwm = std::max(it->second.hwm, it->second.current_bytes);
                ++it->second.count;
            }
        }

        {
            std::lock_guard<std::mutex> g(g_hwm_lock);

            g_active_mem += size;
            g_hwm        = std::max(g_hwm, g_active_mem);
            g_region_hwm = std::max(g_region_hwm, g_active_mem);
        }
    }

    void untrack_mem_cb(Caliper* c, Channel* chn, const void* ptr)
    {
        uint64_t addr = reinterpret_cast<uint64_t>(ptr);
        AllocInfo alloc_info { 0, nullptr };

        {
            std::lock_guard<std::mutex> g(g_alloc_map_lock);
            auto it = g_alloc_map.find(addr);
            if (it == g_alloc_map.end()) {
                ++g_failed_untrack;
                return;
            }
            alloc_info = it->second;
            g_alloc_map.erase(it);
            --g_current_tracked;
        }

        {
            std::lock_guard<std::mutex> g(g_region_map_lock);
            g_region_map[alloc_info.path->id()].current_bytes -= alloc_info.bytes;
        }

        {
            std::lock_guard<std::mutex> g(g_hwm_lock);
            g_active_mem -= alloc_info.bytes;
        }
    }

    void flush_cb(Caliper* c, Channel* channel, SnapshotView, SnapshotFlushFn flush_fn) {
        Attribute alloc_hwm_attr =
            c->create_attribute("alloc.hwm", CALI_TYPE_UINT,
                CALI_ATTR_AGGREGATABLE | CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);
        Attribute alloc_count_attr =
            c->create_attribute("alloc.count", CALI_TYPE_UINT,
                CALI_ATTR_AGGREGATABLE | CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);
        Attribute avg_alloc_size_attr =
            c->create_attribute("avg#alloc.size", CALI_TYPE_DOUBLE,
                CALI_ATTR_AGGREGATABLE | CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);

        std::lock_guard<std::mutex> g(g_region_map_lock);

        for (const auto& it : g_region_map) {
            std::vector<Entry> rec;
            rec.reserve(4);
            rec.emplace_back(it.second.path);
            rec.emplace_back(alloc_hwm_attr, cali_make_variant_from_uint(it.second.hwm));
            rec.emplace_back(alloc_count_attr, cali_make_variant_from_uint(it.second.count));
            double avg_alloc_size = static_cast<double>(it.second.total_bytes) / static_cast<double>(it.second.count);
            rec.emplace_back(avg_alloc_size_attr, Variant(avg_alloc_size));
            flush_fn(*c, rec);
        }

        Log(1).stream() << channel->name() << ": allocsize: flushed " << g_region_map.size() << " records\n";
    }

    void clear_cb(Caliper*, Channel*) {
        std::lock_guard<std::mutex> g(g_region_map_lock);
        g_region_map.clear();
    }

    void finish_cb(Caliper* c, Channel* chn)
    {
        Log(1).stream() << chn->name() << ": allocsize: " << g_total_tracked << " memory allocations tracked (max "
                        << g_max_tracked << " simultaneous), " << g_failed_untrack << " untrack lookups failed."
                        << std::endl;
    }

    AllocSizeService(Caliper* c, Channel* chn)
    { }

public:

    static const char* s_spec;

    static void allocsizeservice_initialize(Caliper* c, Channel* chn)
    {
        AllocSizeService* instance = new AllocSizeService(c, chn);

        chn->events().track_mem_evt.connect([instance](
                                                Caliper*         c,
                                                Channel*         chn,
                                                const void*      ptr,
                                                const char*      label,
                                                size_t           elem_size,
                                                size_t           ndims,
                                                const size_t*    dims,
                                                size_t           n,
                                                const Attribute* attrs,
                                                const Variant*   vals
                                            ) {
            instance->track_mem_cb(c, chn, ptr, label, elem_size, ndims, dims, n, attrs, vals);
        });
        chn->events().untrack_mem_evt.connect([instance](Caliper* c, Channel* chn, const void* ptr) {
            instance->untrack_mem_cb(c, chn, ptr);
        });
        chn->events().flush_evt.connect([instance](Caliper* c, Channel* chn, SnapshotView ctx, SnapshotFlushFn flush_fn){
            instance->flush_cb(c, chn, ctx, flush_fn);
        });
        chn->events().finish_evt.connect([instance](Caliper* c, Channel* chn) {
            instance->finish_cb(c, chn);
            delete instance;
        });

        Log(1).stream() << chn->name() << ": Registered allocsize service" << std::endl;
    }
};

const char* AllocSizeService::s_spec = R"json(
{
 "name" : "allocsize",
 "description" : "Track memory high-water mark per region"
}
)json";

} // namespace

namespace cali
{

CaliperService allocsize_service { ::AllocSizeService::s_spec, ::AllocSizeService::allocsizeservice_initialize };

}
