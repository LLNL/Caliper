/// @file  EventTrigger.cpp
/// @brief Caliper event trigger

#include "../CaliperService.h"

#include <Caliper.h>
#include <SigsafeRWLock.h>

#include <Log.h>
#include <RuntimeConfig.h>

#include <util/split.hpp>

#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <vector>

using namespace cali;
using namespace std;


namespace 
{

const ConfigSet::Entry   configdata[] = {
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

ConfigSet                config;

bool                     enable_snapshot_info;

typedef std::map<cali_id_t, Attribute> AttributeMap;

SigsafeRWLock            level_attributes_lock;
AttributeMap             level_attributes;

std::vector<std::string> trigger_attr_names;

Attribute                trigger_begin_attr { Attribute::invalid };
Attribute                trigger_end_attr   { Attribute::invalid };
Attribute                trigger_set_attr   { Attribute::invalid };

Attribute                trigger_level_attr { Attribute::invalid };

void create_attribute_cb(Caliper* c, const Attribute& attr)
{
    if (attr.skip_events())
        return;

    std::vector<std::string>::iterator it = find(trigger_attr_names.begin(), trigger_attr_names.end(), attr.name());

    if (it != trigger_attr_names.end() || trigger_attr_names.empty()) {
        std::string name = "cali.lvl.";
        name.append(std::to_string(attr.id()));

        Attribute lvl_attr = 
            c->create_attribute(name, CALI_TYPE_INT, 
                                CALI_ATTR_ASVALUE     | 
                                CALI_ATTR_HIDDEN      | 
                                CALI_ATTR_SKIP_EVENTS | 
                                (attr.properties() & CALI_ATTR_SCOPE_MASK));

        level_attributes_lock.wlock();
        level_attributes.insert(std::make_pair(attr.id(), lvl_attr));
        level_attributes_lock.unlock();
    }
}

Attribute get_level_attribute(const Attribute& attr)
{
    Attribute lvl_attr(Attribute::invalid);

    level_attributes_lock.rlock();

    AttributeMap::const_iterator it = level_attributes.find(attr.id());
    if (it != level_attributes.end())
        lvl_attr = it->second;

    level_attributes_lock.unlock();

    return lvl_attr;    
}

void event_begin_cb(Caliper* c, const Attribute& attr)
{
    Attribute lvl_attr(get_level_attribute(attr));

    if (lvl_attr == Attribute::invalid)
        return;

    if (enable_snapshot_info) {
        unsigned  lvl = 1;
        Variant v_lvl(lvl), v_p_lvl;

        // Use Caliper::exchange() to accelerate common-case of setting new hierarchy level to 1.
        // If previous level was > 0, we need to increment it further

        // FIXME: There may be a race condition between c->exchange() and c->set()
        // when two threads update a process-scope attribute.
        // Can fix that with a more general c->update(update_fn) function

        v_p_lvl = c->exchange(lvl_attr, v_lvl);
        lvl     = v_p_lvl.to_uint();

        if (lvl > 0) {
            v_lvl = Variant(++lvl);
            c->set(lvl_attr, v_lvl);
        }

        // Construct the trigger info entry

        Attribute attrs[2] = { trigger_level_attr, trigger_begin_attr };
        Variant   vals[2]  = { v_lvl, Variant(attr.id()) };

        Entry trigger_info = c->make_entry(2, attrs, vals);

        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);
    } else {
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
    }
}

void event_set_cb(Caliper* c, const Attribute& attr)
{
    Attribute lvl_attr(get_level_attribute(attr));

    if (lvl_attr == Attribute::invalid)
        return;

    if (enable_snapshot_info) {
        unsigned  lvl(1);
        Variant v_lvl(lvl);

        // The level for set() is always 1
        // FIXME: ... except for set_path()??
        c->set(lvl_attr, v_lvl);

        // Construct the trigger info entry

        Attribute attrs[2] = { trigger_level_attr, trigger_set_attr };
        Variant   vals[2]  = { v_lvl, Variant(attr.id()) };

        Entry trigger_info = c->make_entry(2, attrs, vals);

        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);
    } else {
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
    }
}

void event_end_cb(Caliper* c, const Attribute& attr)
{
    Attribute lvl_attr(get_level_attribute(attr));

    if (lvl_attr == Attribute::invalid)
        return;

    if (enable_snapshot_info) {
        unsigned  lvl = 0;
        Variant v_lvl(lvl), v_p_lvl;

        // Use Caliper::exchange() to accelerate common-case of setting new level to 0.
        // If previous level was > 1, we need to update it again

        v_p_lvl = c->exchange(lvl_attr, v_lvl);
        lvl     = v_p_lvl.to_uint();

        if (lvl > 1)
            c->set(lvl_attr, Variant(--lvl));

        // Construct the trigger info entry with previous level

        Attribute attrs[2] = { trigger_level_attr, trigger_end_attr };
        Variant   vals[2]  = { v_p_lvl, Variant(attr.id()) };

        Entry trigger_info = c->make_entry(2, attrs, vals);

        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &trigger_info);
    } else {
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
    }
}

void event_trigger_register(Caliper* c) {
    // parse the configuration & set up triggers

    config = RuntimeConfig::init("event", configdata);

    util::split(config.get("trigger").to_string(), ':', 
                std::back_inserter(trigger_attr_names));

    enable_snapshot_info = config.get("enable_snapshot_info").to_bool();

    // register trigger events

    if (enable_snapshot_info) {
        trigger_begin_attr = 
            c->create_attribute("cali.snapshot.event.begin", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        trigger_set_attr = 
            c->create_attribute("cali.snapshot.event.set",   CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        trigger_end_attr = 
            c->create_attribute("cali.snapshot.event.end",   CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        trigger_level_attr = 
            c->create_attribute("cali.snapshot.event.attr.level", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
    }

    // register callbacks

    c->events().create_attr_evt.connect(&create_attribute_cb);

    c->events().pre_begin_evt.connect(&event_begin_cb);
    c->events().pre_set_evt.connect(&event_set_cb);
    c->events().pre_end_evt.connect(&event_end_cb);

    Log(1).stream() << "Registered event trigger service" << endl;
}

} // namespace

namespace cali
{
    CaliperService EventTriggerService { "event", &::event_trigger_register };
}
