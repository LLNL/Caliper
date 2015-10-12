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
#include <set>
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

std::set<cali_id_t>      trigger_attr_ids;
std::vector<std::string> trigger_attr_names;

SigsafeRWLock            trigger_list_lock;

Attribute                trigger_begin_attr { Attribute::invalid };
Attribute                trigger_end_attr   { Attribute::invalid };
Attribute                trigger_set_attr   { Attribute::invalid };

void create_attribute_cb(Caliper* c, const Attribute& attr)
{
    std::vector<std::string>::iterator it = find(trigger_attr_names.begin(), trigger_attr_names.end(), attr.name());

    trigger_list_lock.wlock();

    if (it != trigger_attr_names.end())
        trigger_attr_ids.insert(attr.id());

    trigger_list_lock.unlock();
}

void event_snapshot(Caliper* c, const Attribute& attr, const Attribute& trigger_info_attr)
{
    if (!trigger_attr_names.empty()) {
        bool trigger = false;

        trigger_list_lock.rlock();
        trigger = trigger_attr_ids.count(attr.id()) > 0;
        trigger_list_lock.unlock();
        
        if (!trigger)
            return;
    }

    if (enable_snapshot_info) {
        Variant        val(static_cast<uint64_t>(attr.id()));

        Caliper::Entry entry = c->make_entry(1, &trigger_info_attr, &val);

        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &entry);
    } else
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr);
}

void event_begin_cb(Caliper* c, const Attribute& attr)
{
    event_snapshot(c, attr, trigger_begin_attr);
}

void event_set_cb(Caliper* c, const Attribute& attr)
{
    event_snapshot(c, attr, trigger_set_attr);
}

void event_end_cb(Caliper* c, const Attribute& attr)
{
    event_snapshot(c, attr, trigger_end_attr);
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
