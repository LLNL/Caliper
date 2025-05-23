// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Report.cpp
// Generates text reports from Caliper snapshots on flush() events

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/reader/CaliperMetadataDB.h"
#include "caliper/reader/CalQLParser.h"
#include "caliper/reader/QueryProcessor.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"

#include <iostream>

using namespace cali;

namespace
{

class Report
{
    QuerySpec m_spec;
    ConfigSet m_config;

    //
    // --- callback functions
    //

    void write_output(Caliper* c, ChannelBody* chB, SnapshotView flush_info)
    {
        // set format default to table if it hasn't been set in the query config
        if (m_spec.format.opt == QuerySpec::FormatSpec::Default)
            m_spec.format = CalQLParser("format table").spec().format;

        OutputStream stream;
        stream.set_stream(OutputStream::StdOut);

        std::string filename = m_config.get("filename").to_string();
        if (!filename.empty())
            stream.set_filename(filename.c_str(), *c, std::vector<Entry>(flush_info.begin(), flush_info.end()));
        if (m_config.get("append").to_bool() == true)
            stream.set_mode(OutputStream::Mode::Append);

        CaliperMetadataDB db;
        QueryProcessor    queryP(m_spec, stream);

        db.add_attribute_aliases(m_spec.aliases);
        db.add_attribute_units(m_spec.units);

        c->flush(
            chB,
            flush_info,
            [&queryP, &db](CaliperMetadataAccessInterface& in_db, const std::vector<Entry>& rec) {
                queryP.process_record(db, db.merge_snapshot(in_db, rec));
            }
        );

        db.import_globals(*c, c->get_globals(chB));
        queryP.flush(db);
    }

    Report(const QuerySpec& spec, const ConfigSet& cfg)
        : m_spec { spec }, m_config { cfg }
    { }

public:

    ~Report() {}

    static const char* s_spec;

    static void create(Caliper* c, Channel* channel)
    {
        ConfigSet   config = services::init_config_from_spec(channel->config(), s_spec);
        CalQLParser parser(config.get("config").to_string().c_str());

        if (parser.error()) {
            Log(0).stream() << channel->name() << ": Report: config parse error: " << parser.error_msg() << std::endl;
            return;
        }

        Report* instance = new Report(parser.spec(), config);

        channel->events().write_output_evt.connect([instance](Caliper* c, ChannelBody* chB, SnapshotView info) {
            instance->write_output(c, chB, info);
        });
        channel->events().finish_evt.connect([instance](Caliper*, Channel*) { delete instance; });

        Log(1).stream() << channel->name() << ": Registered report service" << std::endl;
    }
};

const char* Report::s_spec = R"json(
{
"name"        : "report",
"description" : "Write output using CalQL query",
"config"      :
[
 {
  "name": "filename",
  "type": "string",
  "description": "File name for report stream",
  "value": "stdout"
 },{
  "name": "append",
  "type": "bool",
  "description": "Append to file instead of overwriting",
  "value": "false"
 },{
  "name": "config",
  "type": "string"
  "description": "CalQL query to generate report",
 }
]}
)json";

} // namespace

namespace cali
{
CaliperService report_service { ::Report::s_spec, ::Report::create };
}
