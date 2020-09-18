// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Report.cpp
// Generates text reports from Caliper snapshots on flush() events


#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/reader/CaliperMetadataDB.h"
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

    void write_output(Caliper* c, Channel* channel, const SnapshotRecord* flush_info) {
        ConfigSet   config(channel->config().init("report", s_configdata));
        CalQLParser parser(config.get("config").to_string().c_str());

        if (parser.error()) {
            Log(0).stream() << channel->name() << ": Report: config parse error: "
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

        CaliperMetadataDB db;
        QueryProcessor queryP(spec, stream);

        db.add_attribute_aliases(spec.aliases);
        db.add_attribute_units(spec.units);
        db.import_globals(*c, c->get_globals(channel));

        c->flush(channel, flush_info, [&queryP,&db](CaliperMetadataAccessInterface& in_db, const std::vector<Entry>& rec){
                queryP.process_record(db, db.merge_snapshot(in_db, rec));
            } );

        queryP.flush(db);
    }

public:

    ~Report()
        { }

    static void create(Caliper* c, Channel* channel) {
        Report* instance = new Report;

        channel->events().write_output_evt.connect(
            [instance](Caliper* c, Channel* channel, const SnapshotRecord* info){
                instance->write_output(c, channel, info);
            });
        channel->events().finish_evt.connect(
            [instance](Caliper*, Channel*){
                delete instance;
            });

        Log(1).stream() << channel->name() << ": Registered report service" << std::endl;
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
