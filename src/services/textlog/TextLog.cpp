// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// TextLog.cpp
// Caliper text log service

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/OutputStream.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/SnapshotTextFormatter.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

using namespace cali;
using namespace std;

namespace
{

class TextLogService
{
    static const ConfigSet::Entry s_configdata[];

    std::mutex                  trigger_attr_mutex;
    typedef std::map<cali_id_t, Attribute> TriggerAttributeMap;
    TriggerAttributeMap         trigger_attr_map;

    std::vector<std::string>    trigger_attr_names;

    std::string                 stream_filename;
    std::string                 formatstr;

    SnapshotTextFormatter       formatter;
    OutputStream                stream;

    Attribute                   set_event_attr;   
    Attribute                   end_event_attr;

    std::mutex                  stream_mutex;

    std::string 
    create_default_formatstring(const std::vector<std::string>& attr_names) {
        if (attr_names.size() < 1)
            return "%time.inclusive.duration%";

        int name_sizes = 0;

        for (const std::string& s : attr_names)
            name_sizes += s.size();

        int w = std::max<int>(0, (80-10-name_sizes-2*attr_names.size()) / attr_names.size());

        std::ostringstream os;

        for (const std::string& s : attr_names)
            os << s << "=%[" << w << "]" << s << "% ";

        os << "%[8r]time.inclusive.duration%";

        return os.str();
    }

    void check_attribute(const Attribute& attr) {
        if (attr.skip_events())
            return;

        std::vector<std::string>::iterator it = 
            std::find(trigger_attr_names.begin(), trigger_attr_names.end(), attr.name());

        if (it != trigger_attr_names.end()) {
            std::lock_guard<std::mutex>
                g(trigger_attr_mutex);
            
            trigger_attr_map.insert(std::make_pair(attr.id(), attr));
        }
    }

    void process_snapshot(Caliper* c, const SnapshotRecord* trigger_info, const SnapshotRecord* snapshot) {
        // operate only on cali.snapshot.event.end attributes for now
        if (!trigger_info)
            return;

        Entry event = trigger_info->get(end_event_attr);

        if (event.is_empty())
            event = trigger_info->get(set_event_attr);
        if (event.is_empty())
            return;

        Attribute trigger_attr { Attribute::invalid };

        {
            std::lock_guard<std::mutex>
                g(trigger_attr_mutex);

            TriggerAttributeMap::const_iterator it = 
                trigger_attr_map.find(event.value().to_id());

            if (it != trigger_attr_map.end())
                trigger_attr = it->second;
        }

        if (trigger_attr == Attribute::invalid || snapshot->get(trigger_attr).is_empty())
            return;

        auto rec = snapshot->to_entrylist();

        std::lock_guard<std::mutex>
            g(stream_mutex);

        if (!stream)
            stream.set_filename(stream_filename.c_str(), *c, rec);

        std::ostream* osptr = stream.stream();

        formatter.print(*osptr, *c, rec) << std::endl;
    }

    void post_init(Caliper* c, Channel* chn) {
        if (formatstr.size() == 0)
            formatstr = create_default_formatstring(trigger_attr_names);

        formatter.reset(formatstr);

        set_event_attr = c->get_attribute("cali.event.set");
        end_event_attr = c->get_attribute("cali.event.end");

        if (end_event_attr      == Attribute::invalid ||
            set_event_attr      == Attribute::invalid) {
            Log(1).stream() << "TextLog: Note: \"event\" trigger attributes not registered\n"
                "    disabling text log.\n" << std::endl;
            return;
        }

        for (const Attribute& a : c->get_all_attributes())
            check_attribute(a);

        chn->events().process_snapshot.connect(
            [this](Caliper* c, Channel* chn, const SnapshotRecord* info, const SnapshotRecord* rec){
                process_snapshot(c, info, rec);
            });
    }

    TextLogService(Caliper* c, Channel* chn)
        : set_event_attr(Attribute::invalid),
          end_event_attr(Attribute::invalid)
        {
            ConfigSet config = chn->config().init("textlog", s_configdata);
            
            trigger_attr_names =
                config.get("trigger").to_stringlist(",:");
            stream_filename =
                config.get("filename").to_string();
            formatstr =
                config.get("formatstring").to_string();
        }

public:

    static void textlog_register(Caliper* c, Channel* chn) {
        TextLogService* instance = new TextLogService(c, chn);

        chn->events().create_attr_evt.connect(
            [instance](Caliper* c, Channel* chn, const Attribute& attr){
                instance->check_attribute(attr);
            });
        chn->events().post_init_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->post_init(c, chn);
            });
        chn->events().finish_evt.connect(
            [instance](Caliper* c, Channel* chn){
                delete instance;
            });
        
        Log(1).stream() << chn->name() << ": Registered text log service" << std::endl;
    }

}; // TextLogService

const ConfigSet::Entry TextLogService::s_configdata[] = {
    { "trigger", CALI_TYPE_STRING, "",
      "List of attributes for which to write text log entries",
      "Colon-separated list of attributes for which to write text log entries."
    },
    { "formatstring", CALI_TYPE_STRING, "",
      "Format of the text log output",
      "Description of the text log format output. If empty, a default one will be created."
    },
    { "filename", CALI_TYPE_STRING, "stdout",
      "File name for event record stream. Auto-generated by default.",
      "File name for event record stream. Either one of\n"
      "   stdout: Standard output stream,\n"
      "   stderr: Standard error stream,\n"
      "   none:   No output,\n"
      " or a file name. The default is stdout\n"
    },
    ConfigSet::Terminator
};

} // namespace

namespace cali
{

CaliperService textlog_service = { "textlog", ::TextLogService::textlog_register };

} // namespace cali
