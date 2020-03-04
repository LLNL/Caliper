// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include <caliper/Caliper.h>
#include <caliper/ChannelController.h>
#include <caliper/ConfigManager.h>

#include <caliper/reader/Aggregator.h>
#include <caliper/reader/CaliperMetadataDB.h>
#include <caliper/reader/CalQLParser.h>
#include <caliper/reader/FormatProcessor.h>
#include <caliper/reader/NestedExclusiveRegionProfile.h>

#include <caliper/common/Log.h>
#include <caliper/common/OutputStream.h>
#include <caliper/common/StringConverter.h>

#include "../../services/Services.h"

#include <unistd.h>

#include <algorithm>
#include <ctime>

#ifdef CALIPER_HAVE_MPI
#include "caliper/cali-mpi.h"
#include <mpi.h>
#endif

#ifdef CALIPER_HAVE_ADIAK
#include <adiak.hpp>
#endif

using namespace cali;

namespace
{

constexpr int spot_format_version = 1;

std::string
make_filename()
{
    char   timestr[16];
    time_t tm = time(NULL);
    strftime(timestr, sizeof(timestr), "%y%m%d-%H%M%S", localtime(&tm));

    return std::string(timestr) + std::to_string(getpid()) + ".cali";
}

#ifdef CALIPER_HAVE_ADIAK
void
write_adiak(CaliperMetadataDB& db, Aggregator& output_agg)
{
    Log(2).stream() << "[spot controller]: Writing adiak output" << std::endl;

    //   Extract / calculate avg inclusive time for paths.
    // Use exclusive processor b/c values are inclusive already.

    NestedExclusiveRegionProfile rp(db, "avg#inclusive#sum#time.duration");
    output_agg.flush(db, rp);

    std::map<std::string, double> nested_region_times;
    double total_time;

    std::tie(nested_region_times, std::ignore, total_time) =
        rp.result();

    // export time profile into adiak

    adiak::value("total_time", total_time, adiak_performance);

    // copy our map into a vector that adiak can process
    std::vector< std::tuple<std::string, double> > tmpvec(nested_region_times.size());

    std::transform(nested_region_times.begin(), nested_region_times.end(), tmpvec.begin(),
                   [](std::pair<std::string,double>&& p){
                       return std::make_tuple(std::move(p.first), p.second);
                   });

    adiak::value("avg#inclusive#sum#time.duration", tmpvec, adiak_performance);
}
#endif

class SpotController : public cali::ChannelController
{
    cali::ConfigManager::Options m_opts;
    bool m_use_mpi;

    /// \brief Perform intermediate aggregation of channel data into \a output_agg
    void aggregate(const std::string& aggcfg, const std::string& groupby, Caliper& c, CaliperMetadataDB& db, Aggregator& output_agg) {
        QuerySpec spec =
            CalQLParser(std::string("aggregate " + aggcfg + " group by " + groupby).c_str()).spec();

        Aggregator agg(spec);

        c.flush(channel(), nullptr, [&db,&agg](CaliperMetadataAccessInterface& in_db, const std::vector<Entry>& rec){
                   EntryList mrec = db.merge_snapshot(in_db, rec);
                   agg.add(db, mrec);
                });

        // write intermediate results into output aggregator
        agg.flush(db, output_agg);
    }

public:

    void
    flush() {
        Log(1).stream() << "[spot controller]: Flushing Caliper data" << std::endl;

        // --- Setup output reduction aggregator (final cross-process aggregation)
        const char* cross_select =
            " *"
            ",min(inclusive#sum#time.duration)"
            ",max(inclusive#sum#time.duration)"
            ",avg(inclusive#sum#time.duration)";
        std::string cross_query =
            std::string("select ")
            + m_opts.query_select("cross", cross_select, false)
            + " group by "
            + m_opts.query_groupby("cross", "prop:nested")
            + " format cali";

        QuerySpec output_spec(CalQLParser(cross_query.c_str()).spec());
        Aggregator output_agg(output_spec);

        CaliperMetadataDB db;
        Caliper c;

        // ---   Flush Caliper buffers into intermediate aggregator to calculate
        //     inclusive times

        {
            std::string local_select  =
                m_opts.query_select("local", "inclusive_sum(sum#time.duration)", false);
            std::string local_groupby =
                m_opts.query_groupby("local", "prop:nested");

            aggregate(local_select, local_groupby, c, db, output_agg);
        }

        // --- Calculate min/max/avg times across MPI ranks

        int rank = 0;

#ifdef CALIPER_HAVE_MPI
        int initialized = 0;
        MPI_Initialized(&initialized);

        if (initialized != 0 && m_use_mpi) {
            Log(2).stream() << "[spot controller]: Performing cross-process aggregation" << std::endl;

            MPI_Comm comm;
            MPI_Comm_dup(MPI_COMM_WORLD, &comm);
            MPI_Comm_rank(comm, &rank);

            //   Do the global cross-process aggregation.
            // aggregate_over_mpi() does all the magic.
            // Result will be in output_agg on rank 0.
            aggregate_over_mpi(db, output_agg, comm);

            MPI_Comm_free(&comm);
        }
#endif

        // --- Write output

        if (rank == 0) {
            Log(2).stream() << "[spot controller]: Writing output" << std::endl;

            // import globals from Caliper runtime object
            db.import_globals(c, c.get_globals(channel()));

            std::string spot_metrics;

            for (const auto &op : output_spec.aggregation_ops.list) {
                if (!spot_metrics.empty())
                    spot_metrics.append(",");

                spot_metrics.append(Aggregator::get_aggregation_attribute_name(op));
            }

            std::string spot_opts;

            for (const auto &o : m_opts.enabled_options()) {
                if (!spot_opts.empty())
                    spot_opts.append(",");
                spot_opts.append(o);
            }

            Attribute mtr_attr =
                db.create_attribute("spot.metrics",        CALI_TYPE_STRING, CALI_ATTR_GLOBAL);
            Attribute fmt_attr =
                db.create_attribute("spot.format.version", CALI_TYPE_INT,    CALI_ATTR_GLOBAL);
            Attribute opt_attr =
                db.create_attribute("spot.options",        CALI_TYPE_STRING, CALI_ATTR_GLOBAL);

            // set the spot.metrics value
            db.set_global(mtr_attr, Variant(spot_metrics.c_str()));
            db.set_global(fmt_attr, Variant(spot_format_version));
            db.set_global(opt_attr, Variant(spot_opts.c_str()));

            std::string output = m_opts.get("output", "").to_string();

            if (output == "adiak") {
#ifdef CALIPER_HAVE_ADIAK
                ::write_adiak(db, output_agg);
#else
                Log(0).stream() << "[spot controller]: cannot use adiak output: adiak is not enabled!" << std::endl;
#endif
            } else {
                if (output.empty())
                    output = ::make_filename();

                OutputStream    stream;
                stream.set_filename(output.c_str(), c, c.get_globals());

                FormatProcessor formatter(output_spec, stream);

                output_agg.flush(db, formatter);
                formatter.flush(db);
            }
        }
    }

    SpotController(bool use_mpi, const cali::ConfigManager::Options& opts)
        : ChannelController("spot", 0, {
                { "CALI_SERVICES_ENABLE", "aggregate,event,timestamp" },
                { "CALI_EVENT_ENABLE_SNAPSHOT_INFO", "false" },
                { "CALI_TIMER_INCLUSIVE_DURATION", "false" },
                { "CALI_TIMER_SNAPSHOT_DURATION",  "true" },
                { "CALI_TIMER_UNIT", "sec" },
                { "CALI_CHANNEL_FLUSH_ON_EXIT", "false" },
                { "CALI_CHANNEL_CONFIG_CHECK",  "false" }
            }),
          m_opts(opts),
          m_use_mpi(use_mpi)
        {
#ifdef CALIPER_HAVE_ADIAK
            if (opts.get("output", "").to_string() != "adiak")
                config()["CALI_SERVICES_ENABLE"].append(",adiak_import");
#endif
            m_opts.update_channel_config(config());
        }

    ~SpotController()
        { }
};


cali::ChannelController*
make_spot_controller(const cali::ConfigManager::Options& opts) {
    bool use_mpi = false;
#ifdef CALIPER_HAVE_MPI
    use_mpi = true;
#endif

    if (opts.is_set("aggregate_across_ranks"))
        use_mpi = opts.get("aggregate_across_ranks").to_bool();

    return new SpotController(use_mpi, opts);
}

const char* controller_spec =
    "{"
    " \"name\"        : \"spot\","
    " \"description\" : \"Record a time profile for the Spot web visualization framework\","
    " \"categories\"  : [ \"metric\", \"output\", \"region\" ],"
    " \"options\": "
    " ["
    "  {"
    "   \"name\": \"aggregate_across_ranks\","
    "   \"type\": \"bool\","
    "   \"description\": \"Aggregate results across MPI ranks\""
    "  }"
    " ]"
    "}";

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo spot_controller_info
{
    ::controller_spec, ::make_spot_controller, nullptr
};

}
