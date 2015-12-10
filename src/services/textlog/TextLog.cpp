/// \file  TextLog.cpp
/// \brief Caliper text log service

#include "../CaliperService.h"

#include "SnapshotTextFormatter.h"

#include <Caliper.h>
#include <SigsafeRWLock.h>
#include <Snapshot.h>

#include <Log.h>
#include <RuntimeConfig.h>

#include <util/split.hpp>

#include <algorithm>
#include <functional>
#include <iterator>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <sstream>
#include <vector>

using namespace cali;
using namespace std;

namespace
{

const ConfigSet::Entry   configdata[] = {
    { "trigger", CALI_TYPE_STRING, "",
      "List of attributes for which to write text log entries",
      "Colon-separated list of attributes for which to write text log entries."
    },
    { "formatstring", CALI_TYPE_STRING, "",
      "Format of the text log output",
      "Description of the text log format output. If empty, a default one will be created."
    },
    ConfigSet::Terminator
};

ConfigSet config;

std::mutex                  trigger_attr_mutex;
typedef std::map<cali_id_t, Attribute> TriggerAttributeMap;
TriggerAttributeMap         trigger_attr_map;

std::vector<std::string>    trigger_attr_names;

SnapshotTextFormatter*      formatter;

Attribute set_event_attr      { Attribute::invalid };
Attribute end_event_attr      { Attribute::invalid };


std::string create_default_formatstring(const std::vector<std::string>& attr_names)
{
    int name_sizes = 0;

    for (const std::string& s : attr_names)
        name_sizes += s.size();

    int w = max<int>(0, (80-10-name_sizes-2*attr_names.size()) / attr_names.size());

    std::ostringstream os;

    for (const std::string& s : attr_names)
        os << s << "=%[" << w << "]" << s << "% ";

    os << "%[8r]time.phase.duration%";

    return os.str();
}

void create_attribute_cb(Caliper* c, const Attribute& attr)
{
    formatter->update_attribute(attr);

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
    if (!trigger_info || 
        !(trigger_info->attribute() == end_event_attr.id() || trigger_info->attribute() == set_event_attr.id()))
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

    formatter->print(std::cout, snapshot);

    std::cout << std::endl;
}

void post_init_cb(Caliper* c) 
{
    std::string formatstr = config.get("formatstring").to_string();

    if (formatstr.size() == 0)
        formatstr = create_default_formatstring(trigger_attr_names);

    using std::placeholders::_1;

    formatter = new SnapshotTextFormatter;
    formatter->parse(formatstr, c);

    set_event_attr      = c->get_attribute("cali.snapshot.event.set");
    end_event_attr      = c->get_attribute("cali.snapshot.event.end");

    if (end_event_attr      == Attribute::invalid ||
        set_event_attr      == Attribute::invalid)
        Log(1).stream() << "Warning: \"event\" service with snapshot info\n"
            "    and \"timestamp\" service with phase duration recording\n"
            "    is required for text log." << std::endl;
}

void textlog_register(Caliper* c)
{
    config = RuntimeConfig::init("textlog", configdata);

    util::split(config.get("trigger").to_string(), ':', 
                std::back_inserter(trigger_attr_names));

    c->events().create_attr_evt.connect(&create_attribute_cb);
    c->events().post_init_evt.connect(&post_init_cb);
    c->events().process_snapshot.connect(&process_snapshot_cb);

    Log(1).stream() << "Registered text log service" << std::endl;
}

} // namespace

namespace cali
{
    CaliperService TextLogService = { "textlog", ::textlog_register };
} // namespace cali
