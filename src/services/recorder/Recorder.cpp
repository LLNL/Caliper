// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Recorder.cpp
// Caliper event recorder

#include "caliper/CaliperService.h"

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

const ConfigSet::Entry configdata[] = {
    { "filename", CALI_TYPE_STRING, "",
      "File name for event record stream. Auto-generated by default.",
      "File name for event record stream. Either one of\n"
      "   stdout: Standard output stream,\n"
      "   stderr: Standard error stream.\n"
      " or a file name pattern. By default, a filename is auto-generated.\n"
    },
    { "directory", CALI_TYPE_STRING, "",
      "A directory to write .cali files to.",
      "A directory to write .cali files to. The directory must exist,\n"
      "Caliper does not create it\n"
    },
    ConfigSet::Terminator
};

void write_output_cb(Caliper* c, Channel* chn, const SnapshotRecord* flush_info)
{
    ConfigSet cfg = chn->config().init("recorder", configdata);

    std::string filename  = cfg.get("filename").to_string();
    std::string directory = cfg.get("directory").to_string();

    if (filename.empty())
        filename = cali::util::create_filename();
    if (!directory.empty())
        filename = directory + "/" + filename;

    OutputStream stream;
    stream.set_filename(filename.c_str(), *c, flush_info->to_entrylist());

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

CaliperService recorder_service { "recorder", ::recorder_register };

}
