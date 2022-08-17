// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Report.cpp
// Generates text reports from Caliper snapshots on flush() events


#include "caliper/CaliperService.h"

#include "../Services.h"

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
    //
    // --- callback functions
    //

    void write_output(Caliper* c, Channel* channel, SnapshotView flush_info) {
        ConfigSet config = services::init_config_from_spec(channel->config(), s_spec);
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
            stream.set_filename(filename.c_str(), *c, std::vector<Entry>(flush_info.begin(), flush_info.end()));

        CaliperMetadataDB db;
        QueryProcessor queryP(spec, stream);

        db.add_attribute_aliases(spec.aliases);
        db.add_attribute_units(spec.units);

        c->flush(channel, flush_info, [&queryP,&db](CaliperMetadataAccessInterface& in_db, const std::vector<Entry>& rec){
                queryP.process_record(db, db.merge_snapshot(in_db, rec));
            } );

        db.import_globals(*c, c->get_globals(channel));

        queryP.flush(db);
    }

public:

    ~Report()
        { }

    static const char* s_spec;

    static void create(Caliper* c, Channel* channel) {
        Report* instance = new Report;

        channel->events().write_output_evt.connect(
            [instance](Caliper* c, Channel* channel, SnapshotView info){
                instance->write_output(c, channel, info);
            });
        channel->events().finish_evt.connect(
            [instance](Caliper*, Channel*){
                delete instance;
            });

        Log(1).stream() << channel->name() << ": Registered report service" << std::endl;
    }
};

const char* Report::s_spec = R"json(
{   "name": "report",
    "description": "Write output using CalQL query",
    "config": [
        {   "name": "filename",
            "description": "File name for report stream",
            "type": "string",
            "value": "stdout"
        },
        {   "name": "config",
            "description": "CalQL query to generate report",
            "type": "string"
        }
    ]
}
)json";

} // namespace

namespace cali
{
    CaliperService report_service { ::Report::s_spec, ::Report::create };
}
