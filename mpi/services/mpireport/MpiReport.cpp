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

using namespace cali;

namespace
{

class MpiReport
{
    static std::unique_ptr<MpiReport> s_instance;
    static const ConfigSet::Entry     s_configdata[];

    QuerySpec         m_spec;
    CaliperMetadataDB m_db;

    Aggregator        m_a;
    RecordSelector    m_filter;

    std::string       m_filename;

    void add(Caliper* c, const SnapshotRecord* snapshot) {
        // this function processes our local snapshots during flush:
        //   add them to our local aggregator (m_a)

        SnapshotRecord::Sizes s = snapshot->size();
        SnapshotRecord::Data  d = snapshot->data();

        EntryList rec =
            m_db.merge_snapshot(s.n_nodes,     d.node_entries,
                                s.n_immediate, d.immediate_attr, d.immediate_data,
                                *c);

        if (m_filter.pass(m_db, rec))
            m_a.add(m_db, rec);
    }

    void flush_finish(Caliper* c, const SnapshotRecord* flush_info) {
        MPI_Comm comm;
        MPI_Comm_dup(MPI_COMM_WORLD, &comm);
        int rank;
        MPI_Comm_rank(comm, &rank);

        // do the global cross-process aggregation:
        //   aggregate_over_mpi() does all the magic
        aggregate_over_mpi(m_db, m_a, comm);

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

            m_a.flush(m_db, formatter);
            formatter.flush(m_db);
        }
    }

public:

    MpiReport(const QuerySpec& spec, const std::string& filename)
        : m_spec(spec), m_a(spec), m_filter(spec), m_filename(filename)
        { }

    static void pre_flush_cb(Caliper* c, const SnapshotRecord* flush_info) {
        // check if we can use MPI

        int initialized = 0;
        int finalized   = 0;

        MPI_Initialized(&initialized);
        MPI_Finalized(&finalized);

        if (!initialized || finalized)
            return;

        // set up the reporter

        ConfigSet   config(RuntimeConfig::init("mpireport", s_configdata));
        CalQLParser parser(config.get("config").to_string().c_str());

        if (parser.error()) {
            int rank;
            MPI_Comm_rank(MPI_COMM_WORLD, &rank);

            if (rank == 0)
                Log(0).stream() << "mpireport: config parse error: " << parser.error_msg() << std::endl;

            return;
        }

        s_instance.reset(new MpiReport(parser.spec(), config.get("filename").to_string()));
    }

    static void flush_snapshot_cb(Caliper* c, const SnapshotRecord*, const SnapshotRecord* snapshot) {
        if (!s_instance)
            return;

        s_instance->add(c, snapshot);
    }

    static void flush_finish_cb(Caliper* c, const SnapshotRecord* flush_info) {
        if (!s_instance)
            return;

        s_instance->flush_finish(c, flush_info);
        s_instance.reset();
    }
};

std::unique_ptr<MpiReport> MpiReport::s_instance { nullptr };

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
    ConfigSet::Terminator
};

}

namespace cali
{
    void mpireport_init(Caliper* c) {
        c->events().pre_flush_evt.connect(::MpiReport::pre_flush_cb);
        c->events().write_snapshot.connect(::MpiReport::flush_snapshot_cb);
        c->events().post_write_evt.connect(::MpiReport::flush_finish_cb);
    }
}
