// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Recorder.cpp
// Caliper event recorder

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"

#include "caliper/reader/CaliWriter.h"

#include "../../common/util/file_util.h"

#include <iostream>
#include <string>

using namespace cali;

namespace
{

class Recorder
{
    std::string m_channel_name;
    ConfigSet   m_config;

    void write_output_cb(Caliper* c, ChannelBody* chB, SnapshotView flush_info)
    {
        std::string filename  = m_config.get("filename").to_string();
        std::string directory = m_config.get("directory").to_string();

        if (filename.empty())
            filename = cali::util::create_filename();
        if (!directory.empty())
            filename = directory + "/" + filename;

        OutputStream stream;
        stream.set_filename(filename.c_str(), *c, std::vector<Entry>(flush_info.begin(), flush_info.end()));

        CaliWriter writer(stream);

        c->flush(chB, flush_info, [&writer](CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec) {
            writer.write_snapshot(db, rec);
        });

        writer.write_globals(*c, c->get_globals(chB));

        Log(1).stream() << m_channel_name << ": Recorder: Wrote " << writer.num_written() << " records." << std::endl;
    }

    Recorder(const std::string& chname, const ConfigSet& cfg)
        : m_channel_name { chname}, m_config { cfg }
    { }

public:

    static const char* s_spec;

    static void recorder_register(Caliper* c, Channel* channel)
    {
        auto cfg = services::init_config_from_spec(channel->config(), s_spec);
        Recorder* instance = new Recorder(channel->name(), cfg);

        channel->events().write_output_evt.connect([instance](Caliper* c, ChannelBody* chB, SnapshotView info){
                instance->write_output_cb(c, chB, info);
            });
        channel->events().finish_evt.connect([instance](Caliper*, Channel*){ delete instance; });
    }
};

const char* Recorder::s_spec = R"json(
{
"name"        : "recorder",
"description" : "Write records into .cali file",
"config"      :
[
 {
  "name": "filename",
  "type": "string",
  "description": "Stream or file name. If empty, auto-generate file."
 },{
  "name": "directory",
  "type": "string",
  "description": "Directory to write .cali files to."
 }
]}
)json";

} // namespace [anonymous]

namespace cali
{

CaliperService recorder_service { ::Recorder::s_spec, ::Recorder::recorder_register };

}
