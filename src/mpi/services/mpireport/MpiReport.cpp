// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "MpiEvents.h"

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/cali-mpi.h"

#include "caliper/common/Log.h"
#include "caliper/common/OutputStream.h"
#include "caliper/common/RuntimeConfig.h"

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
    static const ConfigSet::Entry s_configdata[];

    QuerySpec   m_cross_spec;
    QuerySpec   m_local_spec;
    std::string m_filename;

    void write_output_cb(Caliper* c, Channel* channel, const SnapshotRecord* flush_info) {
        // check if we can use MPI

        int initialized = 0;
        int finalized   = 0;

        PMPI_Initialized(&initialized);
        PMPI_Finalized(&finalized);

        if (finalized) {
            Log(0).stream() << channel->name()
                            << ": mpireport: MPI is already finalized. Cannot aggregate output."
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

            if (!m_filename.empty())
                stream.set_filename(m_filename.c_str(), *c, flush_info->to_entrylist());
        }

        collective_flush(stream, *c, *channel, flush_info, m_local_spec, m_cross_spec, comm);

        if (comm != MPI_COMM_NULL)
            MPI_Comm_free(&comm);
    }

    MpiReport(const QuerySpec& cross_spec, const QuerySpec &local_spec, const std::string& filename)
        : m_cross_spec(cross_spec),
          m_local_spec(local_spec),
          m_filename(filename)
        { }

public:

    static void init(Caliper* c, Channel* chn) {
        ConfigSet   config = chn->config().init("mpireport", s_configdata);

        std::string cross_cfg = config.get("config").to_string();
        std::string local_cfg = config.get("local_config").to_string();

        CalQLParser cross_parser(cross_cfg.c_str());
        CalQLParser local_parser(local_cfg.empty() ? cross_cfg.c_str() : local_cfg.c_str());

        for ( const auto *p : { &cross_parser, &local_parser } )
            if (p->error()) {
                Log(0).stream() << chn->name() << ": mpireport: config parse error: "
                                << p->error_msg() << std::endl;

                return;
            }

        MpiReport* instance =
            new MpiReport(cross_parser.spec(), local_parser.spec(), config.get("filename").to_string());

        chn->events().write_output_evt.connect(
            [instance](Caliper* c, Channel* chn, const SnapshotRecord* info){
                instance->write_output_cb(c, chn, info);
            });
        chn->events().finish_evt.connect(
            [instance](Caliper*, Channel*){
                delete instance;
            });

        if (config.get("write_on_finalize").to_bool() == true)
            mpiwrap_get_events(chn).mpi_finalize_evt.connect(
                [](Caliper* c, Channel* chn){
                    c->flush_and_write(chn, nullptr);
                });

        Log(1).stream() << chn->name() << ": Registered mpireport service" << std::endl;
    }
};

const ConfigSet::Entry     MpiReport::s_configdata[] = {
    { "filename", CALI_TYPE_STRING, "stdout",
      "File name for report stream. Default: stdout.",
      "File name for report stream. Either one of\n"
      "   stdout: Standard output stream,\n"
      "   stderr: Standard error stream,\n"
      " or a file name.\n"
    },
    { "config", CALI_TYPE_STRING, "",
      "Cross-process aggregation and report configuration/query specification in CalQL",
      "Cross-process aggregation and report configuration/query specification in CalQL"
    },
    { "local_config", CALI_TYPE_STRING, "",
      "CalQL config for process-local aggregation step",
      "CalQL config for a process-local aggregation step applied before cross-process aggregation"
    },
    { "write_on_finalize", CALI_TYPE_BOOL, "true",
      "Flush Caliper buffers on MPI_Finalize",
      "Flush Caliper buffers on MPI_Finalize"
    },
    ConfigSet::Terminator
};

} // namespace [anonymous]

namespace cali
{

CaliperService mpireport_service = { "mpireport", ::MpiReport::init };

}
