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
#include <map>
#include <mutex>
#include <string>
#include <vector>

using namespace cali;
using namespace std;


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
    
    Attribute                trigger_level_attr { Attribute::invalid };

    Attribute                exp_marker_attr    { Attribute::invalid }; 
    
    std::vector<std::string> trigger_attr_names;

    bool                     enable_snapshot_info;

    Node                     event_root_node;

    //
    // --- Helpers / misc
    //

    void mark_attribute(Caliper* c, Channel* chn, const Attribute& attr) {
        cali_id_t evt_attr_ids[4] = { CALI_INV_ID };

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

        std::string s = "cali.lvl#";
        s.append(attr.name());

        evt_attr_ids[3] =
            c->create_attribute(s, CALI_TYPE_INT,
                                CALI_ATTR_ASVALUE     |
                                CALI_ATTR_HIDDEN      |
                                CALI_ATTR_SKIP_EVENTS |
                                (prop & CALI_ATTR_SCOPE_MASK)).id();
        
        c->make_tree_entry(exp_marker_attr,
                           Variant(CALI_TYPE_USR, evt_attr_ids, sizeof(evt_attr_ids)),
                           c->node(attr.node()->id()));
    }

    void check_attribute(Caliper* c, Channel* chn, const Attribute& attr) {
        if (attr.skip_events())
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

    //
    // --- Callbacks
    //

    void create_attr_cb(Caliper* c, Channel* chn, const Attribute& attr) {
        check_attribute(c, chn, attr);
    }
    
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
            Attribute lvl_attr   = c->get_attribute(evt_info_attr_ids[3]);

            assert(begin_attr != Attribute::invalid);
            assert(lvl_attr   != Attribute::invalid);

            uint64_t  lvl = 1;
            Variant v_lvl(lvl), v_p_lvl;

            // Use Caliper::exchange() to accelerate common-case of setting new hierarchy level to 1.
            // If previous level was > 0, we need to increment it further

            // FIXME: There may be a race condition between c->exchange() and c->set()
            // when two threads update a process-scope attribute.
            // Can fix that with a more general c->update(update_fn) function

            v_p_lvl = c->exchange(chn, lvl_attr, v_lvl);
            lvl     = v_p_lvl.to_uint();

            if (lvl > 0) {
                v_lvl = Variant(++lvl);
                c->set(chn, lvl_attr, v_lvl);
            }

            // Construct the trigger info entry

            Attribute attrs[3] = { trigger_level_attr, trigger_begin_attr, begin_attr };
            Variant    vals[3] = { v_lvl, Variant(attr.id()), value };

            SnapshotRecord::FixedSnapshotRecord<3> trigger_info_data;
            SnapshotRecord trigger_info(trigger_info_data);

            c->make_record(3, attrs, vals, trigger_info, &event_root_node);
            c->push_snapshot(chn, CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);
        } else {
            c->push_snapshot(chn, CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
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
            Attribute lvl_attr = c->get_attribute(evt_info_attr_ids[3]);

            assert(set_attr != Attribute::invalid);
            assert(lvl_attr != Attribute::invalid);
            
            uint64_t  lvl(1);
            Variant v_lvl(lvl);

            // The level for set() is always 1
            // FIXME: ... except for set_path()??
            c->set(chn, lvl_attr, v_lvl);

            // Construct the trigger info entry

            Attribute attrs[3] = { trigger_level_attr, trigger_set_attr, set_attr };
            Variant    vals[3] = { v_lvl, Variant(attr.id()), value };

            SnapshotRecord::FixedSnapshotRecord<3> trigger_info_data;
            SnapshotRecord trigger_info(trigger_info_data);

            c->make_record(3, attrs, vals, trigger_info, &event_root_node);
            c->push_snapshot(chn, CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);
        } else {
            c->push_snapshot(chn, CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
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
            Attribute lvl_attr = c->get_attribute(evt_info_attr_ids[3]);

            uint64_t  lvl = 0;
            Variant v_lvl(lvl), v_p_lvl;

            // Use Caliper::exchange() to accelerate common-case of setting new level to 0.
            // If previous level was > 1, we need to update it again

            v_p_lvl = c->exchange(chn, lvl_attr, v_lvl);

            if (v_p_lvl.empty())
                return;

            lvl     = v_p_lvl.to_uint();

            if (lvl > 1)
                c->set(chn, lvl_attr, Variant(--lvl));

            // Construct the trigger info entry with previous level

            Attribute attrs[3] = { trigger_level_attr, trigger_end_attr, end_attr };
            Variant    vals[3] = { v_p_lvl, Variant(attr.id()), value };

            SnapshotRecord::FixedSnapshotRecord<3> trigger_info_data;
            SnapshotRecord trigger_info(trigger_info_data);

            c->make_record(3, attrs, vals, trigger_info, &event_root_node);
            c->push_snapshot(chn, CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);
        } else {
            c->push_snapshot(chn, CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
        }
    } 

    //
    // --- Constructor
    //

    void check_existing_attributes(Caliper* c, Channel* chn) {
        auto attributes = c->get_all_attributes();

        for (const Attribute& attr : attributes)
            check_attribute(c, chn, attr);
    }

    EventTrigger(Caliper* c, Channel* chn)
        : event_root_node(CALI_INV_ID, CALI_INV_ID, Variant())
        {
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
            trigger_level_attr =
                c->create_attribute("cali.event.attr.level",
                                    CALI_TYPE_UINT,
                                    CALI_ATTR_SKIP_EVENTS |
                                    CALI_ATTR_HIDDEN);
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
                instance->create_attr_cb(c, chn, attr);
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
