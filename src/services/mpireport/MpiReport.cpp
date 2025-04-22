// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"
#include "caliper/MpiEvents.h"

#include "caliper/cali-mpi.h"

#include "caliper/common/Log.h"
#include "caliper/common/OutputStream.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/CaliperMetadataDB.h"
#include "caliper/reader/CalQLParser.h"
#include "caliper/reader/FormatProcessor.h"
#include "caliper/reader/Preprocessor.h"
#include "caliper/reader/RecordSelector.h"

#include <memory>

using namespace cali;

namespace
{

class MpiReport
{
    QuerySpec   m_cross_spec;
    QuerySpec   m_local_spec;
    std::string m_filename;
    bool        m_append_to_file;
    std::string m_channel_name;

    void write_output_cb(Caliper* c, ChannelBody* chB, SnapshotView flush_info)
    {
        // check if we can use MPI

        int initialized = 0;
        int finalized   = 0;

        PMPI_Initialized(&initialized);
        PMPI_Finalized(&finalized);

        if (finalized) {
            Log(0).stream() << m_channel_name << ": mpireport: MPI is already finalized. Cannot aggregate output."
                            << std::endl;
            return;
        }

        int      rank = 0;
        MPI_Comm comm = MPI_COMM_NULL;

        if (initialized) {
            MPI_Comm_dup(MPI_COMM_WORLD, &comm);
            MPI_Comm_rank(comm, &rank);
        }

        OutputStream stream;

        if (rank == 0) {
            stream.set_stream(OutputStream::StdOut);

            if (m_append_to_file)
                stream.set_mode(OutputStream::Mode::Append);
            if (!m_filename.empty())
                stream.set_filename(m_filename.c_str(), *c, std::vector<Entry>(flush_info.begin(), flush_info.end()));
        }

        collective_flush(stream, *c, chB, flush_info, m_local_spec, m_cross_spec, comm);

        if (comm != MPI_COMM_NULL)
            MPI_Comm_free(&comm);
    }

    void connect_mpi_finalize(Channel* channel)
    {
        MpiEvents* events = mpiwrap_get_events(channel);

        if (!events) {
            Log(1).stream() << channel->name() << ": mpireport: mpi service is missing\n";
            return;
        }

        events->mpi_finalize_evt.connect([](Caliper* c, Channel* channel) {
            c->flush_and_write(channel->body(), SnapshotView());
        });
    }

    MpiReport(
        const QuerySpec& cross_spec,
        const QuerySpec& local_spec,
        const std::string& filename,
        bool append,
        const std::string& chname)
        : m_cross_spec(cross_spec), m_local_spec(local_spec), m_filename(filename), m_append_to_file(append), m_channel_name(chname)
    {}

public:

    static const char* s_spec;

    static void init(Caliper* c, Channel* chn)
    {
        ConfigSet config = services::init_config_from_spec(chn->config(), s_spec);

        std::string cross_cfg = config.get("config").to_string();
        std::string local_cfg = config.get("local_config").to_string();

        CalQLParser cross_parser(cross_cfg.c_str());
        CalQLParser local_parser(local_cfg.empty() ? cross_cfg.c_str() : local_cfg.c_str());

        for (const auto* p : { &cross_parser, &local_parser })
            if (p->error()) {
                Log(0).stream() << chn->name() << ": mpireport: config parse error: " << p->error_msg() << std::endl;

                return;
            }

        MpiReport* instance = new MpiReport(
            cross_parser.spec(),
            local_parser.spec(),
            config.get("filename").to_string(),
            config.get("append").to_bool(),
            chn->name()
        );

        chn->events().write_output_evt.connect([instance](Caliper* c, ChannelBody* chB, SnapshotView info) {
            instance->write_output_cb(c, chB, info);
        });
        chn->events().finish_evt.connect([instance](Caliper*, Channel*) { delete instance; });

        if (config.get("write_on_finalize").to_bool() == true) {
            chn->events().post_init_evt.connect([instance](Caliper*, Channel* channel) {
                instance->connect_mpi_finalize(channel);
            });
        }

        Log(1).stream() << chn->name() << ": Registered mpireport service" << std::endl;
    }
};

const char* MpiReport::s_spec = R"json(
{
"name": "mpireport",
"description": "Aggregate data across MPI ranks and write output using CalQL query",
"config":
[
 {
  "name": "filename",
  "description": "File name for report stream",
  "type": "string",
  "value": "stdout"
 },{
  "name": "append",
  "description": "Append to file instead of overwriting",
  "type": "bool",
  "value": "false"
 },{
  "name": "config",
  "description": "CalQL query for cross-process aggregation and formatting",
  "type": "string"
 },{
  "name": "local_config",
  "description": "CalQL query for process-local aggregation step",
  "type": "string"
 },{
  "name": "write_on_finalize",
  "description": "Write output at MPI_Finalize",
  "type": "bool",
  "value": "true"
 }
]}
)json";

} // namespace

namespace cali
{

CaliperService mpireport_service = { ::MpiReport::s_spec, ::MpiReport::init };

}
