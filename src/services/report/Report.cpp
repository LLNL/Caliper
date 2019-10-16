// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Report.cpp
// Generates text reports from Caliper snapshots on flush() events


#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/reader/CalQLParser.h"
#include "caliper/reader/QueryProcessor.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"
#include "caliper/common/RuntimeConfig.h"

#include <iostream>

using namespace cali;

namespace
{

class Report {
    static const ConfigSet::Entry s_configdata[];

    //
    // --- callback functions
    //

    void write_output(Caliper* c, Channel* chn, const SnapshotRecord* flush_info) {
        ConfigSet   config(chn->config().init("report", s_configdata));
        CalQLParser parser(config.get("config").to_string().c_str());

        if (parser.error()) {
            Log(0).stream() << chn->name() << ": Report: config parse error: "
                            << parser.error_msg() << std::endl;
            return;
        }

        QuerySpec   spec(parser.spec());

        // set format default to table if it hasn't been set in the query config
        if (spec.format.opt == QuerySpec::FormatSpec::Default)
            spec.format = CalQLParser("format table").spec().format;

        OutputStream stream;

        stream.set_stream(OutputStream::StdOut);

        std::string filename = config.get("filename").to_string();

        if (!filename.empty())
            stream.set_filename(filename.c_str(), *c, flush_info->to_entrylist());

        QueryProcessor queryP(spec, stream);

        c->flush(chn, flush_info, [&queryP](CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec){
                queryP.process_record(db, rec);
            } );

        queryP.flush(*c);
    }

public:

    ~Report()
        { }

    static void create(Caliper* c, Channel* chn) {
        Report* instance = new Report;

        chn->events().write_output_evt.connect(
            [instance](Caliper* c, Channel* chn, const SnapshotRecord* info){
                instance->write_output(c, chn, info);
            });
        chn->events().finish_evt.connect(
            [instance](Caliper*, Channel*){
                delete instance;
            });

        Log(1).stream() << chn->name() << ": Registered report service" << std::endl;
    }
};

const ConfigSet::Entry  Report::s_configdata[] = {
    { "filename", CALI_TYPE_STRING, "stdout",
      "File name for report stream. Default: stdout.",
      "File name for report stream. Either one of\n"
      "   stdout: Standard output stream,\n"
      "   stderr: Standard error stream,\n"
      " or a file name.\n"
    },
    { "config", CALI_TYPE_STRING, "",
      "Report configuration/query specification in CalQL",
      "Report configuration/query specification in CalQL"
    },
    ConfigSet::Terminator
};

} // namespace

namespace cali
{
    CaliperService report_service { "report", ::Report::create };
}
