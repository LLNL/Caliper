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

#include <unistd.h>

#include <algorithm>
#include <ctime>

#include "caliper/cali-mpi.h"

#include <mpi.h>

#ifdef CALIPER_HAVE_ADIAK
#include <adiak.hpp>
#endif

using namespace cali;

namespace
{

enum ProfileConfig {
    MemHighWaterMark = 1
};

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
    std::string m_output;
    int  m_profilecfg;
    bool m_use_mpi;

public:

    void
    flush() {
        Log(1).stream() << "[spot controller]: Flushing Caliper data" << std::endl;

        // --- Setup output reduction aggregator
        std::string select =
            " *"
            ",min(inclusive#sum#time.duration)"
            ",max(inclusive#sum#time.duration)"
            ",avg(inclusive#sum#time.duration)";

        if (m_profilecfg & MemHighWaterMark)
            select.append(",max(max#max#alloc.region.highwatermark)");

        QuerySpec output_spec = CalQLParser((std::string(" select ") + select + " format cali").c_str()).spec();

        Aggregator output_agg(output_spec);

        CaliperMetadataDB db;
        Caliper c;

        // ---   Flush Caliper buffers into intermediate aggregator to calculate
        //     inclusive times

        {
            std::string aggcfg = "inclusive_sum(sum#time.duration)";

            if (m_profilecfg & MemHighWaterMark)
                aggcfg.append(",max(max#alloc.region.highwatermark)");

            QuerySpec  inclusive_spec =
                CalQLParser(std::string("aggregate " + aggcfg + " group by prop:nested").c_str()).spec();

            Aggregator inclusive_agg(inclusive_spec);

            c.flush(channel(), nullptr, [&db,&inclusive_agg](CaliperMetadataAccessInterface& in_db,
                                                             const std::vector<Entry>& rec){
                        EntryList mrec = db.merge_snapshot(in_db, rec);
                        inclusive_agg.add(db, mrec);
                    });

            // write intermediate results into output aggregator
            inclusive_agg.flush(db, output_agg);
        }

        // --- Calculate min/max/avg times across MPI ranks

        int rank = 0;

#ifdef CALIPER_HAVE_MPI
        if (m_use_mpi) {
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

            std::string spot_metrics =
                "avg#inclusive#sum#time.duration"
                ",min#inclusive#sum#time.duration"
                ",max#inclusive#sum#time.duration";

            if (m_profilecfg & MemHighWaterMark)
                spot_metrics.append(",max#max#max#alloc.region.highwatermark");

            // set the spot.metrics value
            db.set_global(db.create_attribute("spot.metrics", CALI_TYPE_STRING, CALI_ATTR_GLOBAL),
                          Variant(CALI_TYPE_STRING, spot_metrics.data(), spot_metrics.length()));

            std::string output = m_output;

            if (output == "adiak") {
#ifdef CALIPER_HAVE_ADIAK
                ::write_adiak(db, output_agg);
#else
                Log(0).stream() << "[spot controller]: cannot use adiak output: adiak is not enabled!" << std::endl;
#endif
            } else {
                if (m_output.empty())
                    output = ::make_filename();

                OutputStream    stream;
                stream.set_filename(output.c_str(), c, c.get_globals());

                FormatProcessor formatter(output_spec, stream);

                output_agg.flush(db, formatter);
                formatter.flush(db);
            }
        }
    }

    SpotController(bool use_mpi, int profilecfg, const char* output)
        : ChannelController("spot", 0, {
                { "CALI_SERVICES_ENABLE", "aggregate,event,timestamp" },
                { "CALI_EVENT_ENABLE_SNAPSHOT_INFO", "false" },
                { "CALI_TIMER_INCLUSIVE_DURATION", "false" },
                { "CALI_TIMER_SNAPSHOT_DURATION",  "true" },
                { "CALI_TIMER_UNIT", "sec" },
                { "CALI_CHANNEL_FLUSH_ON_EXIT", "false" },
                { "CALI_CHANNEL_CONFIG_CHECK",  "false" }
            }),
          m_output(output),
          m_profilecfg(profilecfg),
          m_use_mpi(use_mpi)
        {
#ifdef CALIPER_HAVE_ADIAK
            if (m_output != "adiak")
                config()["CALI_SERVICES_ENABLE"].append(",adiak_import");
#endif
            if (profilecfg & MemHighWaterMark) {
                config()["CALI_SERVICES_ENABLE"].append(",alloc,sysalloc");

                config()["CALI_ALLOC_TRACK_ALLOCATIONS"   ] = "false";
                config()["CALI_ALLOC_RECORD_HIGHWATERMARK"] = "true";
            }
        }

    ~SpotController()
        { }
};

const char* spot_args[] = {
    "output",
    "aggregate_across_ranks",
    "profile",
    "mem.highwatermark",
    nullptr
};

cali::ChannelController*
make_spot_controller(const cali::ConfigManager::argmap_t& args) {
    auto it = args.find("output");
    std::string output = (it == args.end() ? "" : it->second);

    bool use_mpi = false;
#ifdef CALIPER_HAVE_MPI
    use_mpi = true;
#endif

    it = args.find("aggregate_across_ranks");
    if (it != args.end())
        use_mpi = StringConverter(it->second).to_bool();

    int profilecfg = 0;

    it = args.find("profile");
    if (it != args.end() && it->second == "mem.highwatermark")
        profilecfg |= MemHighWaterMark;

    it = args.find("mem.highwatermark");
    if (it != args.end() && StringConverter(it->second).to_bool())
        profilecfg |= MemHighWaterMark;

    return new SpotController(use_mpi, profilecfg, output.c_str());
}

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo spot_controller_info { "spot", "spot(output=<filename>,mpi=true|false): Write Spot output", ::spot_args, ::make_spot_controller };

}
