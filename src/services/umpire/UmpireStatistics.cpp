// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <umpire/ResourceManager.hpp>
#include <umpire/Allocator.hpp>

using namespace cali;


namespace cali
{

extern cali::Attribute class_aggregatable_attr;

}

namespace
{

class UmpireService
{
    Attribute m_alloc_name_attr;
    Attribute m_alloc_current_size_attr;
    Attribute m_alloc_actual_size_attr;
    Attribute m_alloc_hwm_attr;
    Attribute m_total_size_attr;

    Node      m_root_node;

    void process_allocator(Caliper* c, Channel* channel, const std::string& name, umpire::Allocator& alloc, SnapshotView context) {
        uint64_t actual_size = alloc.getActualSize();
        uint64_t current_size = alloc.getCurrentSize();
        uint64_t hwm = alloc.getHighWatermark();

        Attribute attr[] = {
            m_alloc_name_attr,
            m_alloc_actual_size_attr,
            m_alloc_current_size_attr,
            m_alloc_hwm_attr
        };
        Variant   data[] = {
            Variant(name.c_str()),
            Variant(actual_size),
            Variant(current_size),
            Variant(hwm)
        };

        FixedSizeSnapshotRecord<64> rec;
        rec.builder().append(context);

        c->make_record(4, attr, data, rec.builder(), &m_root_node);
        channel->events().process_snapshot(c, channel, SnapshotView(), rec.view());
    }

    void snapshot(Caliper* c, Channel* channel, SnapshotView info, SnapshotBuilder& snapshot_rec) {
        auto& rm = umpire::ResourceManager::getInstance();
        auto allocators = rm.getAllocatorNames();

        if (allocators.empty())
            return;

        //   Bit of a hack: We create one record for each allocator for
        // allocator-specific info. This way we can use generic allocator.name
        // and allocator.size attributes. To avoid issues with repeated
        // snapshots in the same spot (e.g. for timestamps) we just grab the
        // context info and move the records directly to postprocessing.

        FixedSizeSnapshotRecord<60> context;
        context.builder().append(info);
        c->pull_context(channel, CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, context.builder());

        uint64_t total_size = 0;

        for (auto s : allocators) {
            auto alloc = rm.getAllocator(s);
            total_size += alloc.getCurrentSize();
            process_allocator(c, channel, s, alloc, context.view());
        }

        snapshot_rec.append(m_total_size_attr, Variant(total_size));
    }

    void finish_cb(Caliper* c, Channel* channel) {
        Log(1).stream() << channel->name() << ": Finished Umpire service"
                        << std::endl;
    }

    void create_attributes(Caliper* c) {
        Variant v_true(true);

        m_alloc_name_attr =
            c->create_attribute("umpire.alloc.name", CALI_TYPE_STRING,
                                CALI_ATTR_SKIP_EVENTS);
        m_alloc_current_size_attr =
            c->create_attribute("umpire.alloc.current.size",
                                CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS,
                                1, &class_aggregatable_attr, &v_true);
        m_alloc_actual_size_attr =
            c->create_attribute("umpire.alloc.actual.size",
                                CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS,
                                1, &class_aggregatable_attr, &v_true);
        m_alloc_hwm_attr =
            c->create_attribute("umpire.alloc.highwatermark",
                                CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS,
                                1, &class_aggregatable_attr, &v_true);
        m_total_size_attr =
            c->create_attribute("umpire.total.size",
                                CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS,
                                1, &class_aggregatable_attr, &v_true);
    }

    UmpireService(Caliper* c, Channel* channel)
        : m_root_node(CALI_INV_ID, CALI_INV_ID, Variant())
        {
            create_attributes(c);
        }

public:

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

} // namespace [anonymous]

namespace cali
{

CaliperService umpire_service = { "umpire", ::UmpireService::umpire_register };

}