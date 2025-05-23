// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include <caliper/Caliper.h>
#include <caliper/ConfigManager.h>
#include <caliper/CustomOutputController.h>

#include <caliper/reader/Aggregator.h>
#include <caliper/reader/CaliperMetadataDB.h>
#include <caliper/reader/CaliWriter.h>
#include <caliper/reader/CalQLParser.h>
#include <caliper/reader/Preprocessor.h>
#include <caliper/reader/RecordSelector.h>

#include <caliper/common/Log.h>
#include <caliper/common/OutputStream.h>

#include "../../common/StringConverter.h"

#include "../../common/util/file_util.h"

using namespace cali;

namespace
{

constexpr int spot_format_version = 2;

//
// Helper functions
//

QuerySpec parse_spec(const char* query)
{
    CalQLParser parser(query);

    if (parser.error()) {
        Log(0).stream() << "[spot controller]: Internal query parse error:\n    " << parser.error_msg()
            << "\n  in query:\n   " << query << std::endl;
    }

    return parser.spec();
}

/// \brief Perform process-local aggregation of channel data into \a output_agg
void local_aggregate(const char* query, Caliper& c, ChannelBody* chB, CaliperMetadataDB& db, Aggregator& output_agg)
{
    QuerySpec spec(parse_spec(query));

    RecordSelector filter(spec);
    Preprocessor   prp(spec);
    Aggregator     agg(spec);

    c.flush(
        chB,
        SnapshotView(),
        [&db, &filter, &prp, &agg](CaliperMetadataAccessInterface& in_db, const std::vector<Entry>& rec) {
            EntryList mrec = prp.process(db, db.merge_snapshot(in_db, rec));

            if (filter.pass(db, mrec))
                agg.add(db, mrec);
        }
    );

    // write intermediate results into output aggregator
    agg.flush(db, output_agg);
}

//
// Timeseries processing
//

void convert_timeseries_option(const ConfigManager::Options& opts, const char* ts_opt_name, std::string& target)
{
    std::string spot_opt_name = "timeseries.";
    spot_opt_name.append(ts_opt_name);

    if (opts.is_set(spot_opt_name.c_str())) {
        if (!target.empty())
            target.append(",");

        target.append(ts_opt_name);
        target.append("=");
        target.append(opts.get(spot_opt_name.c_str()));
    }
}

//   Make a config string for the timeseries ConfigManager from the
// Spot controller's options
std::string get_timeseries_config_string(const ConfigManager::Options& opts)
{
    std::string ret = "spot.timeseries(";
    std::string tsopts;

    if (opts.is_set("timeseries.metrics"))
        tsopts.append(opts.get("timeseries.metrics"));

    convert_timeseries_option(opts, "iteration_interval", tsopts);
    convert_timeseries_option(opts, "time_interval", tsopts);
    convert_timeseries_option(opts, "target_loops", tsopts);

    ret.append(tsopts);
    ret.append(")");

    return ret;
}

struct LoopInfo {
    std::string name;
    int         iterations;
    int         count;
};

LoopInfo get_loop_info(CaliperMetadataAccessInterface& db, const EntryList& rec)
{
    LoopInfo ret { "", 0, 0 };

    Attribute loop_a = db.get_attribute("loop");
    Attribute iter_a = db.get_attribute("max#sum#loop.iterations");
    Attribute lcnt_a = db.get_attribute("max#count");

    for (const Entry& e : rec) {
        if (e.attribute() == iter_a.id())
            ret.iterations = e.value().to_int();
        else if (e.attribute() == lcnt_a.id())
            ret.count = e.value().to_int();
        else {
            Variant v_loop = e.value(loop_a);
            if (!v_loop.empty())
                ret.name = v_loop.to_string();
        }
    }

    return ret;
}

template <typename T>
std::vector<T> augment_vector(const std::vector<T>& orig, std::initializer_list<T> ilist)
{
    std::vector<T> ret;
    ret.reserve(orig.size() + ilist.size());
    ret.assign(orig.begin(), orig.end());
    ret.insert(ret.end(), ilist);
    return ret;
}

class SpotTimeseriesController : public cali::ChannelController
{
    cali::ConfigManager::Options m_opts;

public:

    void timeseries_local_aggregation(
        Caliper&           c,
        CaliperMetadataDB& db,
        const std::string& loopname,
        int                blocksize,
        Aggregator&        output_agg
    )
    {
        std::string q_local =
            " select cali.channel"
            ",loop"
            ",block"
            ",scale(time.duration.ns,1e-9)"
            ",sum(loop.iterations)"
            ",ratio(loop.iterations,time.duration.ns,1e9)"
            " group by cali.channel,loop,block ";

        q_local.append(" let block = truncate(loop.start_iteration,").append(std::to_string(blocksize)).append(") ");
        q_local.append(" where loop.start_iteration,loop=\"").append(loopname).append("\" ");

        std::string query = m_opts.build_query("local", q_local);

        Channel chn = channel();
        if (chn)
            local_aggregate(query.c_str(), c, chn.body(), db, output_agg);
    }

    QuerySpec timeseries_spec()
    {
        const char* q_cross =
            " select cali.channel"
            ",loop"
            ",block"
            ",max(sum#loop.iterations) as \"Iterations\" unit iterations"
            ",max(scale#time.duration.ns) as \"Time (s)\" unit sec"
            ",avg(ratio#loop.iterations/time.duration.ns) as \"Iter/s\" unit iter/s"
            " group by cali.channel,loop,block ";

        std::string query = m_opts.build_query("cross", q_cross);

        return parse_spec(query.c_str());
    }

    void flush() {}

    SpotTimeseriesController(
        const char*                         name,
        const config_map_t&                 initial_cfg,
        const cali::ConfigManager::Options& opts
    )
        : ChannelController(name, 0, initial_cfg), m_opts(opts)
    {
        if (m_opts.is_set("iteration_interval"))
            config()["CALI_LOOP_MONITOR_ITERATION_INTERVAL"] = m_opts.get("iteration_interval");
        else if (m_opts.is_set("time_interval"))
            config()["CALI_LOOP_MONITOR_TIME_INTERVAL"] = m_opts.get("time_interval");
        else
            config()["CALI_LOOP_MONITOR_TIME_INTERVAL"] = "0.5";

        if (m_opts.is_set("target_loops"))
            config()["CALI_LOOP_MONITOR_TARGET_LOOPS"] = m_opts.get("target_loops");

        m_opts.update_channel_config(config());
    }
};

const char* spot_timeseries_spec =
    "{"
    " \"name\"        : \"spot.timeseries\","
    " \"description\" : \"Collect time-series information for loops\","
    " \"categories\"  : [ \"metric\" ],"
    " \"services\"    : [ \"loop_monitor\", \"timer\", \"trace\" ],"
    " \"config\"      : "
    "   { \"CALI_CHANNEL_FLUSH_ON_EXIT\"      : \"false\","
    "     \"CALI_CHANNEL_CONFIG_CHECK\"       : \"false\","
    "     \"CALI_TIMER_UNIT\"                 : \"sec\""
    "   },"
    " \"options\": "
    " ["
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
    "   \"name\": \"target_loops\","
    "   \"type\": \"string\","
    "   \"description\": \"List of loops to target. Default: any top-level loop.\""
    "  }"
    " ]"
    "}";

cali::ChannelController* make_timeseries_controller(
    const char*                         name,
    const config_map_t&                 cfg,
    const cali::ConfigManager::Options& opts
)
{
    return new SpotTimeseriesController(name, cfg, opts);
}

ConfigManager::ConfigInfo spot_timeseries_info { spot_timeseries_spec, make_timeseries_controller, nullptr };

//
// Spot main
//

using Comm = cali::internal::CustomOutputController::Comm;

class SpotController : public cali::internal::CustomOutputController
{
    ConfigManager::Options m_opts;

    std::string m_spot_metrics;
    std::string m_spot_timeseries_metrics;

    ConfigManager m_timeseries_mgr;

    CaliperMetadataDB m_db;
    Attribute         m_channel_attr;

    void process_timeseries(
        SpotTimeseriesController* tsc,
        Caliper&                  c,
        CaliWriter&               writer,
        const LoopInfo&           info,
        Comm&                     comm
    )
    {
        int         iterations = comm.bcast_int(info.iterations);
        int         rec_count  = comm.bcast_int(info.count);
        std::string name       = comm.bcast_str(info.name);

        if (iterations > 0) {
            int nblocks = 20;

            if (m_opts.is_set("timeseries.maxrows"))
                nblocks = StringConverter(m_opts.get("timeseries.maxrows")).to_int();
            if (nblocks <= 0)
                nblocks = rec_count;

            int blocksize = rec_count > nblocks ? iterations / nblocks : 1;

            QuerySpec  spec = tsc->timeseries_spec();
            Aggregator cross_agg(spec);

            m_db.add_attribute_aliases(spec.aliases);
            m_db.add_attribute_units(spec.units);

            tsc->timeseries_local_aggregation(c, m_db, name.c_str(), std::max(blocksize, 1), cross_agg);
            comm.cross_aggregate(m_db, cross_agg);

            if (comm.rank() == 0) {
                // --- Save the timeseries metrics. Should be the same for each
                //   loop, so just clear them before setting them.
                m_spot_timeseries_metrics.clear();

                for (const auto& op : spec.aggregate.list) {
                    if (!m_spot_timeseries_metrics.empty())
                        m_spot_timeseries_metrics.append(",");

                    m_spot_timeseries_metrics.append(Aggregator::get_aggregation_attribute_name(op));
                }

                Variant v_data("timeseries");
                Entry   entry(m_db.make_tree_entry(1, &m_channel_attr, &v_data));

                // --- Write data
                cross_agg.flush(
                    m_db,
                    [&writer, entry](CaliperMetadataAccessInterface& in_db, const std::vector<Entry>& rec) {
                        writer.write_snapshot(in_db, augment_vector(rec, { entry }));
                    }
                );
            }
        }
    }

    void flush_timeseries(Caliper& c, CaliWriter& writer, Comm& comm)
    {
        auto p = m_timeseries_mgr.get_channel("spot.timeseries");

        if (!p) {
            Log(0).stream() << "[spot controller]: Timeseries channel not found!" << std::endl;
            return;
        }

        auto tsc = std::dynamic_pointer_cast<SpotTimeseriesController>(p);

        const char* summary_local_query = "aggregate count(),sum(loop.iterations) where loop group by loop";
        const char* summary_cross_query = "aggregate max(sum#loop.iterations),max(count) group by loop";

        Aggregator summary_cross_agg(CalQLParser(summary_cross_query).spec());

        local_aggregate(summary_local_query, c, tsc->channel_body(), m_db, summary_cross_agg);
        comm.cross_aggregate(m_db, summary_cross_agg);

        std::vector<LoopInfo> infovec;

        summary_cross_agg.flush(m_db, [&infovec](CaliperMetadataAccessInterface& in_db, const EntryList& rec) {
            infovec.push_back(get_loop_info(in_db, rec));
        });

        if (!infovec.empty()) {
            for (const LoopInfo& loopinfo : infovec)
                if (loopinfo.iterations > 0)
                    process_timeseries(tsc.get(), c, writer, loopinfo, comm);
        } else {
            Log(1).stream() << "[spot controller]: No instrumented loops found" << std::endl;
        }
    }

    void flush_regionprofile(Caliper& c, CaliWriter& writer, Comm& comm)
    {
        // --- Setup output reduction aggregator (final cross-process aggregation)
        const char* q_cross =
            " select *"
            ",min(inclusive#sum#time.duration) as \"Min time/rank\" unit sec"
            ",max(inclusive#sum#time.duration) as \"Max time/rank\" unit sec"
            ",avg(inclusive#sum#time.duration) as \"Avg time/rank\" unit sec"
            ",sum(inclusive#sum#time.duration) as \"Total time\"    unit sec"
            " group by path ";

        std::string cross_query = m_opts.build_query("cross", q_cross);

        QuerySpec  output_spec(parse_spec(cross_query.c_str()));
        Aggregator output_agg(output_spec);

        m_db.add_attribute_aliases(output_spec.aliases);
        m_db.add_attribute_units(output_spec.units);

        // ---   Flush Caliper buffers into intermediate aggregator to calculate
        //     region profile inclusive times
        {
            const char* q_local =
                " let sum#time.duration=scale(sum#time.duration.ns,1e-9)"
                " select inclusive_sum(sum#time.duration)"
                " group by path ";

            std::string query = m_opts.build_query("local", q_local);
            local_aggregate(query.c_str(), c, channel_body(), m_db, output_agg);
        }

        // --- Calculate min/max/avg times across MPI ranks
        comm.cross_aggregate(m_db, output_agg);

        if (comm.rank() == 0) {
            // --- Save the spot metrics
            m_spot_metrics.clear();

            for (const auto& op : output_spec.aggregate.list) {
                if (!m_spot_metrics.empty())
                    m_spot_metrics.append(",");

                m_spot_metrics.append(Aggregator::get_aggregation_attribute_name(op));
            }

            Variant v_data("regionprofile");
            Entry   entry(m_db.make_tree_entry(1, &m_channel_attr, &v_data));

            // --- Write region profile
            output_agg.flush(
                m_db,
                [&writer, entry](CaliperMetadataAccessInterface& in_db, const std::vector<Entry>& rec) {
                    writer.write_snapshot(in_db, augment_vector(rec, { entry }));
                }
            );
        }
    }

    void save_spot_metadata()
    {
        std::string spot_channels = "regionprofile";
        std::string spot_opts     = "";

        for (const auto& o : m_opts.enabled_options()) {
            if (!spot_opts.empty())
                spot_opts.append(",");
            spot_opts.append(o);
            if (o == "timeseries")
                spot_channels.append(",timeseries");
        }

        Attribute mtr_attr = m_db.create_attribute("spot.metrics", CALI_TYPE_STRING, CALI_ATTR_GLOBAL);
        Attribute tsm_attr = m_db.create_attribute("spot.timeseries.metrics", CALI_TYPE_STRING, CALI_ATTR_GLOBAL);
        Attribute fmt_attr = m_db.create_attribute("spot.format.version", CALI_TYPE_INT, CALI_ATTR_GLOBAL);
        Attribute opt_attr = m_db.create_attribute("spot.options", CALI_TYPE_STRING, CALI_ATTR_GLOBAL);
        Attribute chn_attr = m_db.create_attribute("spot.channels", CALI_TYPE_STRING, CALI_ATTR_GLOBAL);

        m_db.set_global(mtr_attr, Variant(m_spot_metrics.c_str()));
        m_db.set_global(tsm_attr, Variant(m_spot_timeseries_metrics.c_str()));
        m_db.set_global(fmt_attr, Variant(spot_format_version));
        m_db.set_global(opt_attr, Variant(spot_opts.c_str()));
        m_db.set_global(chn_attr, Variant(spot_channels.c_str()));
    }

    void on_create(Caliper*, Channel&) override
    {
        if (m_timeseries_mgr.error())
            Log(0).stream() << "[spot controller]: Timeseries config error: " << m_timeseries_mgr.error_msg()
                            << std::endl;

        m_timeseries_mgr.start();
    }

    OutputStream create_output_stream()
    {
        std::string outdir = m_opts.get("outdir");
        std::string output = m_opts.get("output");

        if (output.empty())
            output = cali::util::create_filename();
        if (!outdir.empty() && output != "stderr" && output != "stdout")
            output = outdir + std::string("/") + output;

        Caliper      c;
        OutputStream stream;
        stream.set_filename(output.c_str(), c, c.get_globals());

        return stream;
    }

public:

    void collective_flush(OutputStream& stream, Comm& comm) override
    {
        if (!is_instantiated()) {
            Log(0).stream() << name() << ": SpotController: channel not instantiated\n";
            return;
        }

        Log(1).stream() << name() << ": Flushing Caliper data" << std::endl;

        if (stream.type() == OutputStream::None)
            stream = create_output_stream();

        Caliper    c;
        CaliWriter writer(stream);

        flush_regionprofile(c, writer, comm);

        if (m_opts.is_enabled("timeseries"))
            flush_timeseries(c, writer, comm);

        if (comm.rank() == 0) {
            m_db.import_globals(c, c.get_globals(channel_body()));
            save_spot_metadata();
            writer.write_globals(m_db, m_db.get_globals());

            Log(1).stream() << name() << ": Wrote " << writer.num_written() << " records." << std::endl;
        }
    }

    SpotController(const char* name, const config_map_t& initial_cfg, const ConfigManager::Options& opts)
        : cali::internal::CustomOutputController(name, 0, initial_cfg), m_opts(opts)
    {
        m_channel_attr = m_db.create_attribute("spot.channel", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);

#ifdef CALIPER_HAVE_ADIAK
        config()["CALI_SERVICES_ENABLE"].append(",adiak_import");
        config()["CALI_ADIAK_IMPORT_CATEGORIES"] = opts.get("adiak.import_categories", "2,3");
#endif

        if (opts.is_enabled("timeseries")) {
            m_timeseries_mgr.add_config_spec(spot_timeseries_info);
            m_timeseries_mgr.add(get_timeseries_config_string(m_opts).c_str());
        }

        opts.update_channel_config(config());
        opts.update_channel_metadata(metadata());
    }
};

std::string check_spot_timeseries_args(const cali::ConfigManager::Options& opts)
{
    if (opts.is_enabled("timeseries")) {
        // Check if the timeseries options are valid

        cali::ConfigManager tmpmgr;

        tmpmgr.add_config_spec(spot_timeseries_info);
        return tmpmgr.check(get_timeseries_config_string(opts).c_str());
    } else {
        // Warn when a timeseries option is set but timeseries is disabled

        const char* tsopts[] = { "timeseries.maxrows",
                                 "timeseries.iteration_interval",
                                 "timeseries.time_interval",
                                 "timeseries.target_loops",
                                 "timeseries.metrics" };

        for (const char* opt : tsopts)
            if (opts.is_set(opt))
                return std::string(opt) + " is set but the timeseries option is not enabled";
    }

    return "";
}

const char* spot_controller_spec = R"json(
{
 "name"        : "spot",
 "description" : "Record a time profile for the Spot web visualization framework",
 "categories"  : [ "adiak", "metadata", "metric", "output", "region", "event" ],
 "services"    : [ "aggregate", "event", "timer" ],
 "config"      :
 {
  "CALI_CHANNEL_FLUSH_ON_EXIT"      : "false",
  "CALI_CHANNEL_CONFIG_CHECK"       : "false",
  "CALI_EVENT_ENABLE_SNAPSHOT_INFO" : "false",
  "CALI_TIMER_SNAPSHOT_DURATION"    : "true",
  "CALI_TIMER_INCLUSIVE_DURATION"   : "false"
 },
 "defaults"    : { "node.order": "true", "region.count": "true", "time.exclusive" : "true" },
 "options":
 [
  {
   "name": "time.exclusive",
   "type": "bool",
   "category": "metric",
   "description": "Collect exclusive time per region",
   "query":
   [
    {
     "level"  : "local",
     "select" : [ "scale(sum#time.duration.ns,1e-9) as \"Time (exc)\" unit sec" ]
    },{
     "level"  : "cross",
     "select" :
     [
      "min(scale#sum#time.duration.ns) as \"Min time/rank (exc)\" unit sec",
      "max(scale#sum#time.duration.ns) as \"Max time/rank (exc)\" unit sec",
      "avg(scale#sum#time.duration.ns) as \"Avg time/rank (exc)\" unit sec",
      "sum(scale#sum#time.duration.ns) as \"Total time (exc)\" unit sec"
     ]
    }
   ]
  },{
   "name": "time.variance",
   "type": "bool",
   "category": "metric",
   "description": "Compute population variance of time across MPI ranks",
   "query":
   [
    { "level": "cross", "select": [ "variance(inclusive#sum#time.duration) as \"Variance time/rank\"" ] }
   ]
  },{
   "name": "timeseries",
   "type": "bool",
   "description": "Collect time-series data for annotated loops"
  },{
   "name": "timeseries.maxrows",
   "type": "int",
   "description": "Max number of rows in timeseries output. Set to 0 to show all. Default: 20."
  },{
   "name": "timeseries.iteration_interval",
   "type": "int",
   "description": "Measure every N loop iterations in timeseries"
  },{
   "name": "timeseries.time_interval",
   "type": "double",
   "description": "Measure after t seconds in timeseries"
  },{
   "name": "timeseries.target_loops",
   "type": "string",
   "description": "List of loops to target for timeseries measurements. Default: any top-level loop."
  },{
   "name": "timeseries.metrics",
   "type": "string",
   "description": "Metrics to record for timeseries measurements."
  },{
   "name": "outdir",
   "type": "string",
   "description": "Output directory name"
  }
 ]
}
)json";

cali::ChannelController* make_spot_controller(
    const char*                         name,
    const config_map_t&                 initial_cfg,
    const cali::ConfigManager::Options& opts
)
{
    return new SpotController(name, initial_cfg, opts);
}

} // namespace

namespace cali
{

ConfigManager::ConfigInfo spot_controller_info { ::spot_controller_spec,
                                                 ::make_spot_controller,
                                                 ::check_spot_timeseries_args };

}
