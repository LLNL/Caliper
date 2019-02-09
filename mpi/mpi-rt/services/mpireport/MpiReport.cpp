// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

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
#include "caliper/reader/RecordSelector.h"

#include <memory>

using namespace cali;

namespace
{

class MpiReport
{
    static const ConfigSet::Entry s_configdata[];

    QuerySpec   m_spec;
    std::string m_filename;

    void write_output_cb(Caliper* c, Channel* chn, const SnapshotRecord* flush_info) {
        // check if we can use MPI

        int initialized = 0;
        int finalized   = 0;

        PMPI_Initialized(&initialized);
        PMPI_Finalized(&finalized);

        if (!initialized || finalized)
            return;

        CaliperMetadataDB db;

        Aggregator        agg(m_spec);
        RecordSelector    filter(m_spec);

        // flush this rank's caliper data into local aggregator
        c->flush(chn, flush_info, [c,&db,&agg,&filter](const SnapshotRecord* srec){
                SnapshotRecord::Sizes s = srec->size();
                SnapshotRecord::Data  d = srec->data();

                EntryList rec =
                    db.merge_snapshot(s.n_nodes,     d.node_entries,
                                      s.n_immediate, d.immediate_attr, d.immediate_data,
                                      *c);

                if (filter.pass(db, rec))
                    agg.add(db, rec);
            });

        MPI_Comm comm;
        MPI_Comm_dup(MPI_COMM_WORLD, &comm);
        int rank;
        MPI_Comm_rank(comm, &rank);

        // do the global cross-process aggregation:
        //   aggregate_over_mpi() does all the magic
        aggregate_over_mpi(db, agg, comm);

        MPI_Comm_free(&comm);

        // rank 0's aggregator contains the global result:
        //   create a formatter and print it out
        if (rank == 0) {
            // set default formatter to table if it hasn't been set
            if (m_spec.format.opt == QuerySpec::FormatSpec::Default)
                m_spec.format = CalQLParser("format table").spec().format;

            OutputStream    stream;
            stream.set_stream(OutputStream::StdOut);

            if (!m_filename.empty())
                stream.set_filename(m_filename.c_str(), *c, flush_info->to_entrylist());

            FormatProcessor formatter(m_spec, stream);

            agg.flush(db, formatter);
            formatter.flush(db);
        }
    }

    MpiReport(const QuerySpec& spec, const std::string& filename)
        : m_spec(spec), m_filename(filename)
        { }

public:

    static void init(Caliper* c, Channel* chn) {
        ConfigSet   config = chn->config().init("mpireport", s_configdata);
        CalQLParser parser(config.get("config").to_string().c_str());

        if (parser.error()) {
            Log(0).stream() << chn->name() << ": mpireport: config parse error: "
                            << parser.error_msg() << std::endl;

            return;
        }

        MpiReport* instance =
            new MpiReport(parser.spec(), config.get("filename").to_string());

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
