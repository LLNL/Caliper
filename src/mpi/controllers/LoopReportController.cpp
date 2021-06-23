// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include <caliper/Caliper.h>
#include <caliper/ChannelController.h>
#include <caliper/ConfigManager.h>

#include <caliper/reader/Aggregator.h>
#include <caliper/reader/CaliperMetadataDB.h>
#include <caliper/reader/CalQLParser.h>
#include <caliper/reader/FormatProcessor.h>
#include <caliper/reader/Preprocessor.h>
#include <caliper/reader/RecordSelector.h>

#include <caliper/common/Log.h>
#include <caliper/common/OutputStream.h>
#include <caliper/common/StringConverter.h>

#include <algorithm>
#include <cmath>
#include <sstream>

#ifdef CALIPER_HAVE_MPI
#include "caliper/cali-mpi.h"
#include <mpi.h>
#endif

using namespace cali;

namespace
{

struct LoopInfo {
    std::string name;
    int iterations;
    int count;
};

LoopInfo get_loop_info(CaliperMetadataAccessInterface& db, const EntryList& rec)
{
    LoopInfo ret { "", 0, 0 };

    Attribute loop_a = db.get_attribute("loop");
    Attribute iter_a = db.get_attribute("max#sum#loop.iterations");
    Attribute lcnt_a = db.get_attribute("max#count");

    for (const Entry& e : rec) {
        if      (e.attribute() == iter_a.id())
            ret.iterations = e.value().to_int();
        else if (e.attribute() == lcnt_a.id())
            ret.count      = e.value().to_int();
        else {
            Variant v_loop = e.value(loop_a);
            if (!v_loop.empty())
                ret.name = v_loop.to_string();
        }
    }

    return ret;
}


class LoopReportController : public cali::ChannelController
{
    cali::ConfigManager::Options m_opts;
    bool m_use_mpi;
    int  m_rank;
#ifdef CALIPER_HAVE_MPI
    MPI_Comm m_comm;
#endif

    void init_mpi() {
#ifdef CALIPER_HAVE_MPI
        m_use_mpi = true;

        if (m_opts.is_set("aggregate_across_ranks"))
            m_use_mpi = m_opts.get("aggregate_across_ranks").to_bool();

        int initialized = 0;
        MPI_Initialized(&initialized);

        if (!initialized)
            m_use_mpi = false;

        if (m_use_mpi) {
            MPI_Comm_dup(MPI_COMM_WORLD, &m_comm);
            MPI_Comm_rank(m_comm, &m_rank);
        }
#endif
    }

    void finalize_mpi() {
#ifdef CALIPER_HAVE_MPI
        if (m_use_mpi)
            MPI_Comm_free(&m_comm);
#endif
    }

    Aggregator local_aggregate(Caliper& c, CaliperMetadataDB& db, const QuerySpec& spec) {
        RecordSelector filter(spec);
        Preprocessor   prp(spec);
        Aggregator     agg(spec);

        c.flush(channel(), nullptr, [&db,&filter,&prp,&agg](CaliperMetadataAccessInterface& in_db, const EntryList& rec){
                EntryList mrec = prp.process(db, db.merge_snapshot(in_db, rec));

                if (filter.pass(db, mrec))
                    agg.add(db, mrec);
            });

        return agg;
    }

    void cross_aggregate(CaliperMetadataDB& db, Aggregator& agg) {
#ifdef CALIPER_HAVE_MPI
        if (m_use_mpi)
            aggregate_over_mpi(db, agg, m_comm);
#endif
    }

    Aggregator summary_local_aggregation(Caliper& c, CaliperMetadataDB& db) {
        const char* select =
            " loop"
            ",count()"
            ",sum(loop.iterations)"
            ",sum(time.duration)"
            ",min(iter_per_sec)"
            ",max(iter_per_sec)"
            ",avg(iter_per_sec)";

        std::string query = m_opts.build_query("local", {
                { "let",      "iter_per_sec = ratio(loop.iterations,time.duration)" },
                { "select",   select },
                { "group by", "loop" },
                { "where",    "loop" }
            }, false);

        return local_aggregate(c, db, CalQLParser(query.c_str()).spec());
    }

    QuerySpec summary_query() {
        const char* select =
            " loop as Loop"
            ",max(sum#loop.iterations) as \"Iterations\""
            ",max(sum#time.duration)   as \"Time (s)\""
            ",min(min#iter_per_sec) as \"Iter/s (min)\""
            ",max(max#iter_per_sec) as \"Iter/s (max)\""
            ",avg(avg#iter_per_sec) as \"Iter/s (avg)\"";

        std::string query = m_opts.build_query("cross", {
                { "select",    select },
                { "aggregate", "max(count)" },
                { "group by",  "loop" },
                { "format",    "table" },
            });

        return CalQLParser(query.c_str()).spec();
    }

    Aggregator timeseries_local_aggregation(Caliper& c, CaliperMetadataDB& db, const std::string& loopname, int blocksize) {
        const char* select =
            " Block"
            ",sum(time.duration)"
            ",sum(loop.iterations)"
            ",ratio(loop.iterations,time.duration)";

        std::string block =
            std::string("Block = truncate(loop.start_iteration,") + std::to_string(blocksize) + ")";

        std::string query = m_opts.build_query("local", {
                { "let",      block   },
                { "select",   select  },
                { "group by", "Block" },
                { "where",    std::string("loop=\"")+loopname+"\"" }
            }, false);

        return local_aggregate(c, db, CalQLParser(query.c_str()).spec());
    }

    QuerySpec timeseries_spec() {
        const char* select =
            " Block"
            ",max(sum#loop.iterations) as \"Iterations\""
            ",max(sum#time.duration) as \"Time (s)\""
            ",avg(ratio#loop.iterations/time.duration) as \"Iter/s\"";

        std::string query = m_opts.build_query("cross", {
                { "select",   select  },
                { "group by", "Block" },
                { "format",   "table order by Block" }
            });

        CalQLParser parser(query.c_str());

        if (parser.error())
            Log(0).stream() << parser.error_msg() << " " << query << std::endl;

        return CalQLParser(query.c_str()).spec();
    }

    void process_timeseries(Caliper& c, CaliperMetadataDB& db, OutputStream& stream, const LoopInfo& info) {
        int  iterations = 0;
        int  rec_count = 0;
        char namebuf[64];
        memset(namebuf, 0, 64);

        if (m_rank == 0) {
            iterations = info.iterations;
            rec_count  = info.count;

            if (info.name.size() < 64) {
                std::copy(info.name.begin(), info.name.end(), std::begin(namebuf));
            } else {
                Log(0).stream() << channel()->name() << ": Loop name too long (" << info.name << ")" << std::endl;
                iterations = 0;
            }
        }

#ifdef CALIPER_HAVE_MPI
        if (m_use_mpi) {
            MPI_Bcast(&iterations, 1,  MPI_INT,  0, m_comm);
            MPI_Bcast(&rec_count,  1,  MPI_INT,  0, m_comm);
            MPI_Bcast(namebuf,     64, MPI_CHAR, 0, m_comm);
        }
#endif

        if (iterations > 0) {
            int nblocks = 20;

            if (m_opts.is_set("timeseries.maxrows"))
                nblocks = m_opts.get("timeseries.maxrows").to_int();
            if (nblocks <= 0)
                nblocks = rec_count;

            int blocksize = rec_count > nblocks ? iterations / nblocks : 1;

            Aggregator local_agg = timeseries_local_aggregation(c, db, namebuf, std::max(blocksize, 1));
            QuerySpec  spec      = timeseries_spec();
            Aggregator cross_agg(spec);

            local_agg.flush(db, cross_agg);
            cross_aggregate(db, cross_agg);

            if (m_rank == 0) {
                std::ostream* os = stream.stream();
                *os << "\nIteration summary (" << info.name << "):\n-----------------\n\n";

                FormatProcessor formatter(spec, stream);
                cross_agg.flush(db, formatter);
                formatter.flush(db);
            }
        }
    }


public:

    void flush() {
        Caliper c;
        CaliperMetadataDB db;

        Aggregator summary_local_agg  = summary_local_aggregation(c, db);
        QuerySpec  summary_query_spec = summary_query();
        Aggregator summary_cross_agg(summary_query_spec);

        summary_local_agg.flush(db, summary_cross_agg);

        init_mpi();
        cross_aggregate(db, summary_cross_agg);

        OutputStream stream;

        if (m_rank == 0) {
            std::string output = m_opts.get("output").to_string();
            if (output.empty())
                output = "stderr";

            Caliper c;
            stream.set_filename(output.c_str(), c, c.get_globals());
            std::ostream* os = stream.stream();

            bool print_summary = true;
            if (m_opts.is_set("summary"))
                print_summary = m_opts.get("summary").to_bool();

            if (print_summary) {
                *os << "\nLoop summary:\n------------\n\n";

                FormatProcessor formatter(summary_query_spec, stream);
                summary_cross_agg.flush(db, formatter);
                formatter.flush(db);
            }
        }

        bool print_timeseries = true;
        if (m_opts.is_set("timeseries"))
            print_timeseries = m_opts.get("timeseries").to_bool();

        if (print_timeseries) {
            std::vector<LoopInfo> infovec;

            summary_cross_agg.flush(db, [this,&infovec](CaliperMetadataAccessInterface& db, const EntryList& rec){
                    infovec.push_back(get_loop_info(db, rec));
                });

            if (!infovec.empty()) {
                for (const LoopInfo& loopinfo : infovec)
                    if (loopinfo.iterations > 0)
                        process_timeseries(c, db, stream, loopinfo);
            } else {
                Log(1).stream() << channel()->name() << ": No instrumented loops found" << std::endl;
            }
        }

        finalize_mpi();
    }

    LoopReportController(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts)
        : ChannelController(name, 0, initial_cfg),
          m_opts(opts),
          m_use_mpi(false),
          m_rank(0)
#ifdef CALIPER_HAVE_MPI
          , m_comm(MPI_COMM_NULL)
#endif
    {
        if (m_opts.is_set("iteration_interval"))
            config()["CALI_LOOP_MONITOR_ITERATION_INTERVAL"] = m_opts.get("iteration_interval").to_string();
        else if (m_opts.is_set("time_interval"))
            config()["CALI_LOOP_MONITOR_TIME_INTERVAL"] = m_opts.get("time_interval").to_string();
        else
            config()["CALI_LOOP_MONITOR_TIME_INTERVAL"] = "0.5";

        if (m_opts.is_set("target_loops"))
            config()["CALI_LOOP_MONITOR_TARGET_LOOPS" ] = m_opts.get("target_loops").to_string();

        m_opts.update_channel_config(config());
    }

    static ChannelController* create(const char* name, const config_map_t& initial_cfg, const cali::ConfigManager::Options& opts) {
        return new LoopReportController(name, initial_cfg, opts);
    }
};

const char* loop_report_spec =
    "{"
    " \"name\"        : \"loop-report\","
    " \"description\" : \"Print summary and time-series information for loops\","
    " \"categories\"  : [ \"metric\", \"output\" ],"
    " \"services\"    : [ \"loop_monitor\", \"timestamp\", \"trace\" ],"
    " \"config\"      : "
    "   { \"CALI_CHANNEL_FLUSH_ON_EXIT\"      : \"false\","
    "     \"CALI_CHANNEL_CONFIG_CHECK\"       : \"false\","
    "     \"CALI_TIMER_SNAPSHOT_DURATION\"    : \"true\","
    "     \"CALI_TIMER_INCLUSIVE_DURATION\"   : \"false\","
    "     \"CALI_TIMER_UNIT\"                 : \"sec\""
    "   },"
    " \"options\": "
    " ["
    "  {"
    "   \"name\": \"aggregate_across_ranks\","
    "   \"type\": \"bool\","
    "   \"description\": \"Aggregate results across MPI ranks\""
    "  },"
    "  {"
    "   \"name\": \"summary\","
    "   \"type\": \"bool\","
    "   \"description\": \"Print loop summary\""
    "  },"
    "  {"
    "   \"name\": \"timeseries\","
    "   \"type\": \"bool\","
    "   \"description\": \"Print time series\""
    "  },"
    "  {"
    "   \"name\": \"iteration_interval\","
    "   \"type\": \"int\","
    "   \"description\": \"Measure every N loop iterations\""
    "  },"
    "  {"
    "   \"name\": \"time_interval\","
    "   \"type\": \"double\","
    "   \"description\": \"Measure after t seconds\""
    "  },"
    "  {"
    "   \"name\": \"timeseries.maxrows\","
    "   \"type\": \"int\","
    "   \"description\": \"Max number of rows in timeseries display. Set to 0 to show all. Default: 20.\""
    "  },"
    "  {"
    "   \"name\": \"target_loops\","
    "   \"type\": \"string\","
    "   \"description\": \"List of loops to target. Default: any top-level loop.\""
    "  }"
    " ]"
    "}";

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo loop_report_controller_info
{
    ::loop_report_spec, ::LoopReportController::create, nullptr
};

}
