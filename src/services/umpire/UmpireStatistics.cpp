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
    Attribute m_allocator_name_attr;
    Attribute m_current_size_attr;
    Attribute m_actual_size_attr;

    Node      m_root_node;

    void push_allocator_statistics(Caliper* c, Channel* channel, const std::string& name, umpire::Allocator& alloc) {
        uint64_t actual_size = alloc.getActualSize();
        uint64_t current_size = alloc.getCurrentSize();

        Attribute attr[] = {
            m_allocator_name_attr, m_actual_size_attr,   m_current_size_attr
        };
        Variant   data[] = {
            Variant(name.c_str()), Variant(actual_size), Variant(current_size)
        };

        FixedSizeSnapshotRecord<3> rec;
        c->make_record(3, attr, data, rec.builder(), &m_root_node);
        c->push_snapshot(channel, rec.view());
    }

    void snapshot(Caliper* c, Channel* channel, const Attribute& attr, const Variant& val) {
        auto& rm = umpire::ResourceManager::getInstance();

        for (auto s : rm.getAllocatorNames()) {
            auto alloc = rm.getAllocator(s);
            push_allocator_statistics(c, channel, s, alloc);
        }
    }

    void finish_cb(Caliper* c, Channel* channel) {
        Log(1).stream() << channel->name() << ": Finished Umpire service"
                        << std::endl;
    }

    void create_attributes(Caliper* c) {
        Variant v_true(true);

        m_allocator_name_attr =
            c->create_attribute("umpire.alloc.name", CALI_TYPE_STRING,
                                CALI_ATTR_SKIP_EVENTS);
        m_current_size_attr =
            c->create_attribute("umpire.alloc.current.size",
                                CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS,
                                1, &class_aggregatable_attr, &v_true);
        m_actual_size_attr =
            c->create_attribute("umpire.alloc.actual.size",
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

        channel->events().post_begin_evt.connect(
            [instance](Caliper* c, Channel* channel, const Attribute& attr, const Variant& val){
                instance->snapshot(c, channel, attr, val);
            });
        channel->events().pre_end_evt.connect(
            [instance](Caliper* c, Channel* channel, const Attribute& attr, const Variant& val){
                instance->snapshot(c, channel, attr, val);
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