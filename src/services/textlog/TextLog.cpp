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
    std::vector<Attribute>      trigger_attributes;

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
        std::vector<std::string>::iterator it =
            std::find(trigger_attr_names.begin(), trigger_attr_names.end(), attr.name());

        if (it != trigger_attr_names.end()) {
            std::lock_guard<std::mutex>
                g(trigger_attr_mutex);

            trigger_attributes.push_back(attr);
            Log(1).stream() << "textlog: Found " << *it << std::endl;
        }
    }

    bool is_triggering_event(const Attribute& event_attr, const SnapshotRecord* trigger_info) {
        if (event_attr == Attribute::invalid)
            return false;

        Entry event = trigger_info->get(event_attr);

        if (!event.is_empty()) {
            std::lock_guard<std::mutex>
                g(trigger_attr_mutex);

            for (const Attribute& a : trigger_attributes)
                if (event.value().to_id() == a.id())
                    return true;
        }

        return false;
    }

    bool is_triggering_snapshot(const SnapshotRecord* trigger_info) {
        if (!trigger_info)
            return false;

        // check if any of the textlog trigger attributes are in trigger_info

        {
            std::lock_guard<std::mutex>
                g(trigger_attr_mutex);

            for (const Attribute& a : trigger_attributes)
                if (!trigger_info->get(a).is_empty())
                    return true;
        }

        // check if there is a begin or end event with any of the textlog triggers
        return is_triggering_event(end_event_attr, trigger_info) ||
               is_triggering_event(set_event_attr, trigger_info);
    }

    void process_snapshot(Caliper* c, const SnapshotRecord* trigger_info, const SnapshotRecord* snapshot) {
        if (!is_triggering_snapshot(trigger_info))
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
                config.get("trigger").to_stringlist(",");
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
