// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <umpire/ResourceManager.hpp>
#include <umpire/Allocator.hpp>

using namespace cali;


namespace
{

class UmpireService
{
    Attribute m_alloc_name_attr;
    Attribute m_alloc_current_size_attr;
    Attribute m_alloc_actual_size_attr;
    Attribute m_alloc_hwm_attr;
    Attribute m_alloc_count_attr;
    Attribute m_total_size_attr;
    Attribute m_total_count_attr;
    Attribute m_total_hwm_attr;

    Node      m_root_node;

    bool      m_per_allocator_stats;

    void process_allocator(Caliper* c, Channel* channel, const std::string& name, umpire::Allocator& alloc, SnapshotView context) {
        uint64_t actual_size = alloc.getActualSize();
        uint64_t current_size = alloc.getCurrentSize();
        uint64_t hwm = alloc.getHighWatermark();
        uint64_t count = alloc.getAllocationCount();

        Attribute attr[] = {
            m_alloc_name_attr,
            m_alloc_actual_size_attr,
            m_alloc_current_size_attr,
            m_alloc_hwm_attr,
            m_alloc_count_attr
        };
        Variant   data[] = {
            Variant(name.c_str()),
            Variant(actual_size),
            Variant(current_size),
            Variant(hwm),
            Variant(count)
        };

        FixedSizeSnapshotRecord<64> rec;
        rec.builder().append(context);

        c->make_record(5, attr, data, rec.builder(), &m_root_node);
        channel->events().process_snapshot(c, channel, SnapshotView(), rec.view());
    }

    void snapshot(Caliper* c, Channel* channel, SnapshotView info, SnapshotBuilder& rec) {
        //   Bit of a hack: We create one record for each allocator for
        // allocator-specific info. This way we can use generic allocator.name
        // and allocator.size attributes. To avoid issues with repeated
        // snapshots in the same spot (e.g. for timestamps) we just grab the
        // context info and move the records directly to postprocessing.

        FixedSizeSnapshotRecord<60> context;

        if (m_per_allocator_stats) {
            context.builder().append(info);
            c->pull_context(channel, CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, context.builder());
        }

        uint64_t total_size  = 0;
        uint64_t total_count = 0;
        uint64_t total_hwm   = 0;

        auto& rm = umpire::ResourceManager::getInstance();

        for (auto s : rm.getAllocatorNames()) {
            auto alloc = rm.getAllocator(s);

            total_size  += alloc.getCurrentSize();
            total_count += alloc.getAllocationCount();
            total_hwm   += alloc.getHighWatermark();

            if (m_per_allocator_stats)
                process_allocator(c, channel, s, alloc, context.view());
        }

        rec.append(m_total_size_attr,  Variant(total_size));
        rec.append(m_total_count_attr, Variant(total_count));
        rec.append(m_total_hwm_attr,   Variant(total_hwm));
    }

    void finish_cb(Caliper* c, Channel* channel) {
        Log(1).stream() << channel->name() << ": Finished Umpire service"
                        << std::endl;
    }

    void create_attributes(Caliper* c) {
        m_alloc_name_attr =
            c->create_attribute("umpire.alloc.name", CALI_TYPE_STRING,
                                CALI_ATTR_SKIP_EVENTS);
        m_alloc_current_size_attr =
            c->create_attribute("umpire.alloc.current.size",
                                CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE     |
                                CALI_ATTR_SKIP_EVENTS |
                                CALI_ATTR_AGGREGATABLE);
        m_alloc_actual_size_attr =
            c->create_attribute("umpire.alloc.actual.size",
                                CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE     |
                                CALI_ATTR_SKIP_EVENTS |
                                CALI_ATTR_AGGREGATABLE);
        m_alloc_hwm_attr =
            c->create_attribute("umpire.alloc.highwatermark",
                                CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE     |
                                CALI_ATTR_SKIP_EVENTS |
                                CALI_ATTR_AGGREGATABLE);
        m_alloc_count_attr =
            c->create_attribute("umpire.alloc.count",
                                CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE     |
                                CALI_ATTR_SKIP_EVENTS |
                                CALI_ATTR_AGGREGATABLE);
        m_total_size_attr =
            c->create_attribute("umpire.total.size",
                                CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE     |
                                CALI_ATTR_SKIP_EVENTS |
                                CALI_ATTR_AGGREGATABLE);
        m_total_count_attr =
            c->create_attribute("umpire.total.count",
                                CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE     |
                                CALI_ATTR_SKIP_EVENTS |
                                CALI_ATTR_AGGREGATABLE);
        m_total_hwm_attr =
            c->create_attribute("umpire.total.hwm",
                                CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE     |
                                CALI_ATTR_SKIP_EVENTS |
                                CALI_ATTR_AGGREGATABLE);
    }

    UmpireService(Caliper* c, Channel* channel)
        : m_root_node(CALI_INV_ID, CALI_INV_ID, Variant()),
          m_per_allocator_stats(true)
        {
            auto config = services::init_config_from_spec(channel->config(), s_spec);

            m_per_allocator_stats =
                config.get("per_allocator_statistics").to_bool();

            create_attributes(c);
        }

public:

    static const char* s_spec;

    static void umpire_register(Caliper* c, Channel* channel) {
        UmpireService* instance = new UmpireService(c, channel);

        channel->events().snapshot.connect(
            [instance](Caliper* c, Channel* channel, int, SnapshotView info, SnapshotBuilder& rec){
                instance->snapshot(c, channel, info, rec);
            });
        channel->events().finish_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->finish_cb(c, channel);
                delete instance;
            });

        Log(1).stream() << channel->name() << ": Registered umpire service"
                        << std::endl;
    }
};

const char* UmpireService::s_spec = R"json(
{   "name"        : "umpire",
    "description" : "Record Umpire memory manager statistics",
    "config"      :
    [
        {   "name"        : "per_allocator_statistics",
            "description" : "Include statistics for each Umpire allocator",
            "type"        : "bool",
            "value"       : "true"
        }
    ]
}
)json";

} // namespace [anonymous]

namespace cali
{

CaliperService umpire_service = { ::UmpireService::s_spec, ::UmpireService::umpire_register };

}