// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Recorder.cpp
// Caliper event recorder

#include "caliper/CaliperService.h"

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"
#include "caliper/common/RuntimeConfig.h"

#include "caliper/reader/CaliWriter.h"

#include "../../common/util/file_util.h"

#include <iostream>
#include <string>

using namespace cali;
using namespace std;

namespace
{

const char* spec = R"json(
    {   "name"        : "recorder",
        "description" : "Write records into .cali file",
        "config"      : [
            { "name"        : "filename",
              "type"        : "string",
              "description" : "Stream or file name. If empty, auto-generate file."
            },
            { "name"        : "directory",
              "type"        : "string",
              "description" : "Directory to write .cali files to."
            }
        ]
    }
)json";

void write_output_cb(Caliper* c, Channel* chn, SnapshotView flush_info)
{
    ConfigSet cfg = services::init_config_from_spec(chn->config(), spec);

    std::string filename  = cfg.get("filename").to_string();
    std::string directory = cfg.get("directory").to_string();

    if (filename.empty())
        filename = cali::util::create_filename();
    if (!directory.empty())
        filename = directory + "/" + filename;

    OutputStream stream;
    stream.set_filename(filename.c_str(), *c, std::vector<Entry>(flush_info.begin(), flush_info.end()));

    CaliWriter writer(stream);

    c->flush(chn, flush_info, [&writer](CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec){
            writer.write_snapshot(db, rec);
        });

    writer.write_globals(*c, c->get_globals(chn));

    Log(1).stream() << chn->name()
                    << ": Recorder: Wrote " << writer.num_written() << " records." << std::endl;
}

void recorder_register(Caliper* c, Channel* chn)
{
    chn->events().write_output_evt.connect(::write_output_cb);
}

} // namespace

namespace cali
{

CaliperService recorder_service { ::spec, ::recorder_register };

}
