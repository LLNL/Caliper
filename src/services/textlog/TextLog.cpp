// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// TextLog.cpp
// Caliper text log service

#include "caliper/CaliperService.h"

#include "../Services.h"

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

    bool is_triggering_event(const Attribute& event_attr, SnapshotView trigger_info) {
        if (event_attr == Attribute::invalid)
            return false;

        Entry event = trigger_info.get(event_attr);

        if (!event.empty()) {
            cali_id_t id = event.value().to_id();

            std::lock_guard<std::mutex>
                g(trigger_attr_mutex);

            for (const Attribute& a : trigger_attributes)
                if (id == a.id())
                    return true;
        }

        return false;
    }

    bool is_triggering_snapshot(SnapshotView trigger_info) {
        if (trigger_info.empty())
            return false;

        // check if any of the textlog trigger attributes are in trigger_info

        {
            std::lock_guard<std::mutex>
                g(trigger_attr_mutex);

            for (const Attribute& a : trigger_attributes)
                if (!trigger_info.get(a).empty())
                    return true;
        }

        // check if there is a begin or end event with any of the textlog triggers
        return is_triggering_event(end_event_attr, trigger_info) ||
               is_triggering_event(set_event_attr, trigger_info);
    }

    void process_snapshot(Caliper* c, SnapshotView trigger_info, SnapshotView snapshot) {
        if (!is_triggering_snapshot(trigger_info))
            return;

        std::vector<Entry> rec(snapshot.begin(), snapshot.end());

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
            [this](Caliper* c, Channel* chn, SnapshotView info, SnapshotView rec){
                process_snapshot(c, info, rec);
            });
    }

    TextLogService(Caliper* c, Channel* chn)
        : set_event_attr(Attribute::invalid),
          end_event_attr(Attribute::invalid)
        {
            ConfigSet config = services::init_config_from_spec(chn->config(), s_spec);

            trigger_attr_names =
                config.get("trigger").to_stringlist(",");
            stream_filename =
                config.get("filename").to_string();
            formatstr =
                config.get("formatstring").to_string();
        }

public:

    static const char* s_spec;

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

const char* TextLogService::s_spec = R"json(
{   "name": "textlog",
    "description": "Write runtime output for (some) snapshots",
    "config": [
        {   "name": "trigger",
            "type": "string",
            "description": "List of attributes for which to write text log entries",
        },
        {   "name": "formatstring",
            "description": "Format string for the text log output",
            "type": "string"
        },
        {   "name": "filename",
            "type": "string",
            "description": "File or stream to write to",
            "value": "stdout"
        }
    ]
}
)json";

} // namespace

namespace cali
{

CaliperService textlog_service = { ::TextLogService::s_spec, ::TextLogService::textlog_register };

} // namespace cali
