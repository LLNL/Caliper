// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// EventTrigger.cpp
// Caliper event trigger

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

using namespace cali;
using namespace std;

namespace cali
{
    extern Attribute subscription_event_attr; // From api.cpp
}

namespace
{

class EventTrigger
{
    //
    // --- Static data
    //

    static const ConfigSet::Entry s_configdata[];

    //
    // --- Per-channel instance data
    //

    Attribute                trigger_begin_attr { Attribute::invalid };
    Attribute                trigger_end_attr   { Attribute::invalid };
    Attribute                trigger_set_attr   { Attribute::invalid };

    Attribute                exp_marker_attr    { Attribute::invalid };

    Attribute                region_count_attr  { Attribute::invalid };

    std::vector<std::string> trigger_attr_names;

    bool                     enable_snapshot_info;

    Node                     event_root_node;

    //
    // --- Helpers / misc
    //

    void mark_attribute(Caliper* c, Channel* chn, const Attribute& attr) {
        cali_id_t evt_attr_ids[3] = { CALI_INV_ID };

        struct evt_attr_setup_t {
            std::string prefix;
            int         index;
        } evt_attr_setup[] = {
            { "event.begin#", 0 },
            { "event.set#",   1 },
            { "event.end#",   2 }
        };

        cali_attr_type type = attr.type();
        int            prop = attr.properties();

        for ( evt_attr_setup_t setup : evt_attr_setup ) {
            std::string s = setup.prefix;
            s.append(attr.name());

            int delete_flags = CALI_ATTR_NESTED | CALI_ATTR_GLOBAL;

            evt_attr_ids[setup.index] =
                c->create_attribute(s, type, (prop & ~delete_flags) | CALI_ATTR_SKIP_EVENTS).id();
        }

        c->make_tree_entry(exp_marker_attr,
                           Variant(CALI_TYPE_USR, evt_attr_ids, sizeof(evt_attr_ids)),
                           c->node(attr.node()->id()));

        Log(2).stream() << chn->name() << ": event: Marked attribute " << attr.name() << std::endl;
    }

    void check_attribute(Caliper* c, Channel* chn, const Attribute& attr) {
        if (attr.id() < 12 /* skip fixed metadata attributes */ || attr.skip_events())
            return;

        auto it = std::find(trigger_attr_names.begin(), trigger_attr_names.end(), attr.name());

        if (trigger_attr_names.size() > 0 && it == trigger_attr_names.end())
            return;

        mark_attribute(c, chn, attr);
    }

    const Node* find_exp_marker(const Attribute& attr) {
        cali_id_t marker_id = exp_marker_attr.id();

        for (const Node* node = attr.node()->first_child(); node; node = node->next_sibling())
            if (node->attribute() == marker_id)
                return node;

        return nullptr;
    }

    static inline bool is_subscription_attribute(const Attribute& attr) {
        return attr.get(cali::subscription_event_attr).to_bool();
    }

    //
    // --- Callbacks
    //

    void pre_begin_cb(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value) {
        const Node* marker_node = find_exp_marker(attr);

        if (!marker_node)
            return;

        if (enable_snapshot_info) {
            assert(!marker_node->data().empty());

            const cali_id_t* evt_info_attr_ids =
                static_cast<const cali_id_t*>(marker_node->data().data());

            assert(evt_info_attr_ids != nullptr);

            Attribute begin_attr = c->get_attribute(evt_info_attr_ids[0]);

            assert(begin_attr != Attribute::invalid);

            // Construct the trigger info entry

            Attribute attrs[2] = { trigger_begin_attr, begin_attr };
            Variant    vals[2] = { Variant(attr.id()), value };

            SnapshotRecord::FixedSnapshotRecord<2> trigger_info_data;
            SnapshotRecord trigger_info(trigger_info_data);

            c->make_record(2, attrs, vals, trigger_info, &event_root_node);
            c->push_snapshot(chn, &trigger_info);
        } else {
            c->push_snapshot(chn, nullptr);
        }
    }

    void pre_set_cb(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value) {
        const Node* marker_node = find_exp_marker(attr);

        if (!marker_node)
            return;

        if (enable_snapshot_info) {
            assert(!marker_node->data().empty());

            const cali_id_t* evt_info_attr_ids =
                static_cast<const cali_id_t*>(marker_node->data().data());

            assert(evt_info_attr_ids != nullptr);

            Attribute set_attr = c->get_attribute(evt_info_attr_ids[1]);

            assert(set_attr != Attribute::invalid);

            // Construct the trigger info entry

            Attribute attrs[2] = { trigger_set_attr,   set_attr };
            Variant    vals[2] = { Variant(attr.id()), value    };

            SnapshotRecord::FixedSnapshotRecord<2> trigger_info_data;
            SnapshotRecord trigger_info(trigger_info_data);

            c->make_record(2, attrs, vals, trigger_info, &event_root_node);
            c->push_snapshot(chn, &trigger_info);
        } else {
            c->push_snapshot(chn, nullptr);
        }
    }

    void pre_end_cb(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value) {
        const Node* marker_node = find_exp_marker(attr);

        if (!marker_node)
            return;

        if (enable_snapshot_info) {
            assert(!marker_node->data().empty());

            const cali_id_t* evt_info_attr_ids =
                static_cast<const cali_id_t*>(marker_node->data().data());

            assert(evt_info_attr_ids != nullptr);

            Attribute end_attr = c->get_attribute(evt_info_attr_ids[2]);

            // Construct the trigger info entry with previous level

            Attribute attrs[3] = { trigger_end_attr, end_attr, region_count_attr };
            Variant    vals[3] = { Variant(attr.id()), value, cali_make_variant_from_uint(1) };

            SnapshotRecord::FixedSnapshotRecord<3> trigger_info_data;
            SnapshotRecord trigger_info(trigger_info_data);

            c->make_record(3, attrs, vals, trigger_info, &event_root_node);
            c->push_snapshot(chn, &trigger_info);
        } else {
            cali_id_t attr_id = region_count_attr.id();
            Variant   v_1(cali_make_variant_from_uint(1));
            SnapshotRecord trigger_info(1, &attr_id, &v_1);

            c->push_snapshot(chn, &trigger_info);
        }
    }

    //
    // --- Constructor
    //

    void check_existing_attributes(Caliper* c, Channel* chn) {
        auto attributes = c->get_all_attributes();

        for (const Attribute& attr : attributes)
            if (!is_subscription_attribute(attr))
                check_attribute(c, chn, attr);
    }

    EventTrigger(Caliper* c, Channel* chn)
        : event_root_node(CALI_INV_ID, CALI_INV_ID, Variant())
        {
            Attribute aggr_attr = c->get_attribute("class.aggregatable");
            Variant   v_true(true);

            region_count_attr =
                c->create_attribute("region.count", CALI_TYPE_UINT,
                                    CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE,
                                    1, &aggr_attr, &v_true);

            ConfigSet cfg =
                chn->config().init("event", s_configdata);

            trigger_attr_names   = cfg.get("trigger").to_stringlist(",:");
            enable_snapshot_info = cfg.get("enable_snapshot_info").to_bool();

            // register trigger events

            trigger_begin_attr =
                c->create_attribute("cali.event.begin",
                                    CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_HIDDEN);
            trigger_set_attr =
                c->create_attribute("cali.event.set",
                                    CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_HIDDEN);
            trigger_end_attr =
                c->create_attribute("cali.event.end",
                                    CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_HIDDEN);
            exp_marker_attr =
                c->create_attribute(std::string("event.exp#")+std::to_string(chn->id()),
                                    CALI_TYPE_USR,
                                    CALI_ATTR_SKIP_EVENTS |
                                    CALI_ATTR_HIDDEN);

            check_existing_attributes(c, chn);
        }

public:

    static void event_trigger_register(Caliper* c, Channel* chn) {
        EventTrigger* instance = new EventTrigger(c, chn);

        chn->events().create_attr_evt.connect(
            [instance](Caliper* c, Channel* chn, const Attribute& attr){
                if (!is_subscription_attribute(attr))
                    instance->check_attribute(c, chn, attr);
            });
        chn->events().subscribe_attribute.connect(
            [instance](Caliper* c, Channel* chn, const Attribute& attr){
                instance->check_attribute(c, chn, attr);
            });
        chn->events().pre_begin_evt.connect(
            [instance](Caliper* c, Channel* chn, const Attribute& attr, const Variant& value){
                instance->pre_begin_cb(c, chn, attr, value);
            });
        chn->events().pre_set_evt.connect(
            [instance](Caliper* c, Channel* chn, const Attribute& attr, const Variant& value){
                instance->pre_set_cb(c, chn, attr, value);
            });
        chn->events().pre_end_evt.connect(
            [instance](Caliper* c, Channel* chn, const Attribute& attr, const Variant& value){
                instance->pre_end_cb(c, chn, attr, value);
            });
        chn->events().finish_evt.connect(
            [instance](Caliper*, Channel*){
                delete instance;
            });

        Log(1).stream() << chn->name() << ": Registered event trigger service" << std::endl;
    }
};

const ConfigSet::Entry EventTrigger::s_configdata[] = {
    { "trigger", CALI_TYPE_STRING, "",
      "List of attributes that trigger measurement snapshots.",
      "Colon-separated list of attributes that trigger measurement snapshots.\n"
      "If empty, all user attributes trigger measurement snapshots."
    },
    { "enable_snapshot_info", CALI_TYPE_BOOL, "true",
      "Enable snapshot info records",
      "Enable snapshot info records."
    },

    ConfigSet::Terminator
};

} // namespace


namespace cali
{

CaliperService event_service { "event", ::EventTrigger::event_trigger_register };

}
