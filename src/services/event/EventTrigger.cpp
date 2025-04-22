// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// EventTrigger.cpp
// Caliper event trigger

#include "../Services.h"

#include "../../caliper/RegionFilter.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

using namespace cali;

namespace cali
{
extern Attribute subscription_event_attr; // From api.cpp
extern Attribute phase_attr;
} // namespace cali

namespace
{

class EventTrigger
{
    //
    // --- Per-channel instance data
    //

    Attribute trigger_begin_attr;
    Attribute trigger_end_attr;
    Attribute trigger_set_attr;

    Attribute marker_attr;

    Attribute region_count_attr;
    Entry     region_count_entry;

    std::vector<std::string> trigger_attr_names;

    bool enable_snapshot_info { true };
    int  region_level { 0 };

    Node event_root_node;

    RegionFilter region_filter;
    RegionFilter branch_filter;

    std::vector<Variant> branch_filter_stack;

    std::string channel_name;

    //
    // --- Helpers / misc
    //

    void parse_region_level(const std::string& str)
    {
        if (str == "phase") {
            region_level = phase_attr.level();
        } else {
            bool ok    = false;
            int  level = StringConverter(str).to_int(&ok);

            if (!ok || level < 0 || level > 7) {
                Log(0).stream() << channel_name << ": event: Invalid region level \"" << str << "\"\n";
                region_level = 0;
            } else {
                region_level = level;
            }
        }

        Log(2).stream() << channel_name << ": event: Using region level " << region_level << "\n";
    }

    void mark_attribute(Caliper* c, const Attribute& attr)
    {
        cali_attr_type type = attr.type();
        int            prop = attr.properties();

        int delete_flags = CALI_ATTR_NESTED | CALI_ATTR_GLOBAL;
        int flags = (prop & ~delete_flags) | CALI_ATTR_SKIP_EVENTS;

        Variant v_id(cali_make_variant_from_uint(attr.id()));

        Attribute begin_attr =
            c->create_attribute(std::string("event.begin#")+attr.name(), type, flags,
                1, &trigger_begin_attr, &v_id);
        Attribute set_attr =
            c->create_attribute(std::string("event.set#")+attr.name(), type, flags,
                1, &trigger_set_attr, &v_id);
        Attribute end_attr =
            c->create_attribute(std::string("event.end#")+attr.name(), type, flags,
                1, &trigger_end_attr, &v_id);

        cali_id_t evt_attr_ids[3] = { begin_attr.id(), set_attr.id(), end_attr.id() };

        c->make_tree_entry(marker_attr, Variant(CALI_TYPE_USR, evt_attr_ids, sizeof(evt_attr_ids)), attr.node());

        Log(2).stream() << channel_name << ": event: Marked attribute " << attr.name() << std::endl;
    }

    void check_attribute(Caliper* c, const Attribute& attr)
    {
        if (attr.id() < 12 /* skip fixed metadata attributes */ || attr.skip_events())
            return;

        auto it = std::find(trigger_attr_names.begin(), trigger_attr_names.end(), attr.name());

        if (trigger_attr_names.size() > 0 && it == trigger_attr_names.end())
            return;
        if (attr.level() < region_level)
            return;

        mark_attribute(c, attr);
    }

    const Node* find_marker(const Attribute& attr)
    {
        cali_id_t marker_id = marker_attr.id();

        for (const Node* node = attr.node()->first_child(); node; node = node->next_sibling())
            if (node->attribute() == marker_id)
                return node;

        return nullptr;
    }

    static inline bool is_subscription_attribute(const Attribute& attr)
    {
        return attr.get(cali::subscription_event_attr).to_bool();
    }

    //
    // --- Callbacks
    //

    void pre_begin_cb(Caliper* c, ChannelBody* chB, const Attribute& attr, const Variant& value)
    {
        const Node* marker_node = find_marker(attr);

        if (!marker_node)
            return;
        if (attr.type() == CALI_TYPE_STRING && !region_filter.pass(value))
            return;
        if (branch_filter.has_filters()) {
            if (branch_filter.pass(value))
                branch_filter_stack.push_back(value);
            if (branch_filter_stack.empty())
                return;
        }

        if (enable_snapshot_info) {
            assert(!marker_node->data().empty());
            const cali_id_t* evt_info_attr_ids = static_cast<const cali_id_t*>(marker_node->data().data());
            assert(evt_info_attr_ids != nullptr);

            Attribute begin_attr = c->get_attribute(evt_info_attr_ids[0]);

            // Construct the trigger info entry
            if (attr.store_as_value()) {
                Entry info(begin_attr, value);
                c->push_snapshot(chB, SnapshotView(info));
            } else {
                //   As an optimization we attach the trigger info entries directly to their
                // current context node. This drastically reduces the search space in the
                // metadata tree as otherwise all trigger info nodes anywhere would be in a
                // very wide branch under a single root node which has to be sequentially
                // searched. We also now occupy one less slot in the snapshot record.
                //   To do this we use the special-purpose push_snapshot_replace() function
                // to replace the original blackboard entry in the snapshot record with the
                // augmented entry in trigger_info.
                Entry bb_entry = c->get_blackboard_entry(attr);
                Entry info(c->make_tree_entry(begin_attr, value, bb_entry.empty() ? &event_root_node : bb_entry.node()));
                c->push_snapshot_replace(chB, SnapshotView(info), bb_entry);
            }
        } else {
            c->push_snapshot(chB, SnapshotView());
        }
    }

    void pre_set_cb(Caliper* c, ChannelBody* chB, const Attribute& attr, const Variant& value)
    {
        const Node* marker_node = find_marker(attr);

        if (!marker_node)
            return;
        if (attr.type() == CALI_TYPE_STRING && !region_filter.pass(value))
            return;
        if (branch_filter.has_filters()) {
            if (branch_filter.pass(value))
                branch_filter_stack.push_back(value);
            if (branch_filter_stack.empty())
                return;
        }

        if (enable_snapshot_info) {
            assert(!marker_node->data().empty());
            const cali_id_t* evt_info_attr_ids = static_cast<const cali_id_t*>(marker_node->data().data());
            assert(evt_info_attr_ids != nullptr);

            Attribute set_attr = c->get_attribute(evt_info_attr_ids[1]);

            // Construct the trigger info entry
            if (attr.store_as_value()) {
                Entry info(set_attr, value);
                c->push_snapshot(chB, SnapshotView(info));
            } else {
                Entry bb_entry = c->get_blackboard_entry(attr);
                Entry info(c->make_tree_entry(set_attr, value, bb_entry.empty() ? &event_root_node : bb_entry.node()));
                c->push_snapshot_replace(chB, SnapshotView(info), bb_entry);
            }
        } else {
            c->push_snapshot(chB, SnapshotView());
        }
    }

    void pre_end_cb(Caliper* c, ChannelBody* chB, const Attribute& attr, const Variant& value)
    {
        const Node* marker_node = find_marker(attr);

        if (!marker_node)
            return;
        if (attr.type() == CALI_TYPE_STRING && !region_filter.pass(value))
            return;
        if (branch_filter.has_filters()) {
            if (branch_filter_stack.empty())
                return;
            if (value == branch_filter_stack.back())
                branch_filter_stack.pop_back();
        }

        if (enable_snapshot_info) {
            assert(!marker_node->data().empty());
            const cali_id_t* evt_info_attr_ids = static_cast<const cali_id_t*>(marker_node->data().data());
            assert(evt_info_attr_ids != nullptr);

            Attribute end_attr = c->get_attribute(evt_info_attr_ids[2]);

            // Construct the trigger info entry
            if (attr.store_as_value()) {
                Entry info[2] = { Entry(end_attr, value), region_count_entry };
                c->push_snapshot(chB, SnapshotView(2, info));
            } else {
                Entry bb_entry = c->get_blackboard_entry(attr);
                Node* node = c->make_tree_entry(end_attr, value, bb_entry.node());
                Entry info[2] = { Entry(node), region_count_entry };
                c->push_snapshot_replace(chB, SnapshotView(2, info), bb_entry);
            }
        } else {
            c->push_snapshot(chB, SnapshotView(region_count_entry));
        }
    }

    //
    // --- Constructor
    //

    void check_existing_attributes(Caliper* c)
    {
        auto attributes = c->get_all_attributes();

        for (const Attribute& attr : attributes)
            if (!is_subscription_attribute(attr))
                check_attribute(c, attr);
    }

    EventTrigger(Caliper* c, Channel* channel) 
        : event_root_node(CALI_INV_ID, CALI_INV_ID, Variant()), channel_name { channel->name() }
    {
        region_count_attr = c->create_attribute(
            "region.count",
            CALI_TYPE_UINT,
            CALI_ATTR_SKIP_EVENTS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        region_count_entry = Entry(region_count_attr, cali_make_variant_from_uint(1));

        ConfigSet cfg = services::init_config_from_spec(channel->config(), s_spec);

        trigger_attr_names   = cfg.get("trigger").to_stringlist(",:");
        enable_snapshot_info = cfg.get("enable_snapshot_info").to_bool();
        parse_region_level(cfg.get("region_level").to_string());

        {
            std::string i_filter = cfg.get("include_regions").to_string();
            std::string e_filter = cfg.get("exclude_regions").to_string();

            auto p = RegionFilter::from_config(i_filter, e_filter);

            if (!p.second.empty()) {
                Log(0).stream() << channel->name() << ": event: filter parse error: " << p.second << std::endl;
            } else {
                region_filter = p.first;
            }
        }

        {
            std::string i_filter = cfg.get("include_branches").to_string();

            auto p = RegionFilter::from_config(i_filter, "");

            if (!p.second.empty()) {
                Log(0).stream() << channel->name() << ": event: branch filter parse error: " << p.second << std::endl;
            } else {
                branch_filter = p.first;
            }
        }

        // register trigger events

        trigger_begin_attr =
            c->create_attribute("cali.event.begin", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_HIDDEN);
        trigger_set_attr =
            c->create_attribute("cali.event.set", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_HIDDEN);
        trigger_end_attr =
            c->create_attribute("cali.event.end", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_HIDDEN);
        marker_attr = c->create_attribute(
            std::string("event.marker#") + std::to_string(channel->id()),
            CALI_TYPE_USR,
            CALI_ATTR_SKIP_EVENTS | CALI_ATTR_HIDDEN
        );

        check_existing_attributes(c);
    }

public:

    static const char* s_spec;

    static void event_trigger_register(Caliper* c, Channel* chn)
    {
        EventTrigger* instance = new EventTrigger(c, chn);

        chn->events().create_attr_evt.connect([instance](Caliper* c, const Attribute& attr) {
            if (!is_subscription_attribute(attr))
                instance->check_attribute(c, attr);
        });
        chn->events().subscribe_attribute.connect([instance](Caliper* c, const Attribute& attr) {
            instance->check_attribute(c, attr);
        });
        chn->events().pre_begin_evt.connect(
            [instance](Caliper* c, ChannelBody* chB, const Attribute& attr, const Variant& value) {
                instance->pre_begin_cb(c, chB, attr, value);
            }
        );
        chn->events().pre_set_evt.connect(
            [instance](Caliper* c, ChannelBody* chB, const Attribute& attr, const Variant& value) {
                instance->pre_set_cb(c, chB, attr, value);
            }
        );
        chn->events().pre_end_evt.connect(
            [instance](Caliper* c, ChannelBody* chB, const Attribute& attr, const Variant& value) {
                instance->pre_end_cb(c, chB, attr, value);
            }
        );
        chn->events().finish_evt.connect([instance](Caliper*, Channel*) { delete instance; });

        Log(1).stream() << chn->name() << ": Registered event trigger service" << std::endl;
    }
};

const char* EventTrigger::s_spec = R"json(
{
 "name"        : "event",
 "description" : "Trigger snapshots for Caliper region begin and end events",
 "config"      :
 [
  {
   "name": "trigger",
   "type": "stringlist",
   "description": "List of attributes that trigger measurements (optional)"
  },{
   "name": "region_level",
   "type": "string",
   "description": "Minimum region level that triggers snapshots",
   "value": "0"
  },{
   "name": "enable_snapshot_info",
   "type": "bool",
   "description": "If true, add begin/end attributes at each event. Increases overhead.",
   "value": "True"
  },{
   "name": "include_regions",
   "type": "string",
   "description": "Region filter to specify regions that will trigger snapshots"
  },{
   "name": "exclude_regions",
   "type": "string",
   "description": "Region filter to specify regions that won't trigger snapshots"
  },{
   "name": "include_branches",
   "type": "string",
   "description": "Region filter to specify a branch"
  }
 ]
}
)json";

} // namespace

namespace cali
{

CaliperService event_service { ::EventTrigger::s_spec, ::EventTrigger::event_trigger_register };

}
