/// @file  ProgressLog.cpp
/// @brief Caliper progress log service


#include "../CaliperService.h"

#include <Caliper.h>
#include <SigsafeRWLock.h>
#include <Snapshot.h>

#include <Log.h>
#include <RuntimeConfig.h>

#include <util/split.hpp>

#include <algorithm>
#include <iterator>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

using namespace cali;
using namespace std;

namespace
{

const ConfigSet::Entry   configdata[] = {
    { "trigger", CALI_TYPE_STRING, "",
      "List of attributes for which to write progress log entries",
      "Colon-separated list of attributes for which to write progress log entries."
    },
    ConfigSet::Terminator
};

ConfigSet config;

std::mutex                  trigger_attr_mutex;
typedef std::map<cali_id_t, Attribute> TriggerAttributeMap;
TriggerAttributeMap         trigger_attr_map;

std::vector<std::string>    trigger_attr_names;

Attribute end_event_attr      { Attribute::invalid };
Attribute phase_duration_attr { Attribute::invalid };


void create_attribute_cb(Caliper* c, const Attribute& attr)
{
    if (attr.skip_events())
        return;

    std::vector<std::string>::iterator it = 
        find(trigger_attr_names.begin(), trigger_attr_names.end(), attr.name());

    if (it != trigger_attr_names.end()) {
        std::lock_guard<std::mutex> lock(trigger_attr_mutex);
        trigger_attr_map.insert(std::make_pair(attr.id(), attr));
    }
}

void process_snapshot_cb(Caliper* c, const Entry* trigger_info, const Snapshot* snapshot)
{
    // operate only on cali.snapshot.event.end attributes for now
    if (!trigger_info || trigger_info->attribute() != end_event_attr.id())
        return;

    Attribute trigger_attr { Attribute::invalid };

    {
        std::lock_guard<std::mutex> lock(trigger_attr_mutex);

        TriggerAttributeMap::const_iterator it = 
            trigger_attr_map.find(trigger_info->value().to_id());

        if (it != trigger_attr_map.end())
            trigger_attr = it->second;
    }

    if (trigger_attr == Attribute::invalid)
        return;

    Entry time_entry = snapshot->get(phase_duration_attr);
    Entry attr_entry = snapshot->get(trigger_attr);


    // make 22:48:10 attribute:value:time entries for now

    struct message_field_t {
        std::string            str;
        std::string::size_type width;
    } message_fields[] = {
        { trigger_attr.name(),            22 },
        { attr_entry.value().to_string(), 48 },
        { time_entry.value().to_string(), 10 }
    };

    static const char whitespace[80+1] = 
        "                                        "
        "                                        ";

    for (const message_field_t& f : message_fields)
        std::cout << f.str << (f.str.size() < f.width ? whitespace+(80-f.width+f.str.size()) : "");

    std::cout << std::endl;
}

void post_init_cb(Caliper* c) 
{
    end_event_attr      = c->get_attribute("cali.snapshot.event.end");
    phase_duration_attr = c->get_attribute("time.phase.duration");

    if (end_event_attr      == Attribute::invalid ||
        phase_duration_attr == Attribute::invalid) 
        Log(1).stream() << "Warning: \"event\" service with snapshot info\n"
            "    and \"timestamp\" service with phase duration recording\n"
            "    is required for progress log." << std::endl;

    std::cout << "Phase                 " 
              << "Value                                           "
              << "Time      "
              << std::endl;
}

void progress_log_register(Caliper* c)
{
    config = RuntimeConfig::init("progresslog", configdata);

    util::split(config.get("trigger").to_string(), ':', 
                std::back_inserter(trigger_attr_names));

    c->events().create_attr_evt.connect(&create_attribute_cb);
    c->events().post_init_evt.connect(&post_init_cb);
    c->events().process_snapshot.connect(&process_snapshot_cb);

    Log(1).stream() << "Registered progress log service" << std::endl;
}

} // namespace

namespace cali
{
    CaliperService ProgressLogService = { "progresslog", ::progress_log_register };
} // namespace cali
