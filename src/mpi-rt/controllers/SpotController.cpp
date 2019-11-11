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

#include "caliper/cali-mpi.h"

#include <mpi.h>

#ifdef CALIPER_HAVE_ADIAK
#include <adiak.hpp>
#endif

using namespace cali;

namespace
{

constexpr int spot_format_version = 1;

enum ProfileConfig {
    MemHighWaterMark = 1,
    IoBytes = 2,
    IoBandwidth = 4,
    WrapMpi = 8,
    WrapCuda = 16
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
        std::string select =
            " *"
            ",min(inclusive#sum#time.duration)"
            ",max(inclusive#sum#time.duration)"
            ",avg(inclusive#sum#time.duration)";

        if (m_profilecfg & MemHighWaterMark)
            select.append(",max(max#max#alloc.region.highwatermark)");
        if (m_profilecfg & IoBytes) {
            select.append(",avg(sum#sum#io.bytes.written)");
            select.append(",avg(sum#sum#io.bytes.read)");
            select.append(",sum(sum#sum#io.bytes.written)");
            select.append(",sum(sum#sum#io.bytes.read)");
        }
        if (m_profilecfg & IoBandwidth) {
            select.append(",avg(sum#io.bytes.written/sum#time.duration)");
            select.append(",avg(sum#io.bytes.read/sum#time.duration)");
            select.append(",max(sum#io.bytes.written/sum#time.duration)");
            select.append(",max(sum#io.bytes.read/sum#time.duration)");
        }

        QuerySpec output_spec =
            CalQLParser(std::string("select " + select + " group by prop:nested format cali").c_str()).spec();

        Aggregator output_agg(output_spec);

        CaliperMetadataDB db;
        Caliper c;

        // ---   Flush Caliper buffers into intermediate aggregator to calculate
        //     inclusive times

        {
            std::string agg = "inclusive_sum(sum#time.duration)";

            if (m_profilecfg & MemHighWaterMark)
                agg.append(",max(max#alloc.region.highwatermark)");
            if (m_profilecfg & IoBytes)
                agg.append(",sum(sum#io.bytes.read),sum(sum#io.bytes.written)");

            aggregate(agg, "prop:nested", c, db, output_agg);
        }

        // --- Calculate per-rank bandwidths in I/O regions. This needs a separate
        //   step because we need to group by io.region, too (it's complicated ...)
        if (m_profilecfg & IoBandwidth) {
            std::string agg =
                " ratio(sum#io.bytes.written,sum#time.duration,8e-6)"
                ",ratio(sum#io.bytes.read,   sum#time.duration,8e-6)";

            aggregate(agg, "prop:nested,io.region", c, db, output_agg);
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
            if (m_profilecfg & IoBytes) {
                spot_metrics.append(",avg#sum#sum#io.bytes.written");
                spot_metrics.append(",avg#sum#sum#io.bytes.read");
                spot_metrics.append(",sum#sum#sum#io.bytes.written");
                spot_metrics.append(",sum#sum#sum#io.bytes.read");
            }
            if (m_profilecfg & IoBandwidth) {
                spot_metrics.append(",avg#sum#io.bytes.written/sum#time.duration");
                spot_metrics.append(",avg#sum#io.bytes.read/sum#time.duration");
                spot_metrics.append(",max#sum#io.bytes.written/sum#time.duration");
                spot_metrics.append(",max#sum#io.bytes.read/sum#time.duration");
            }

            Attribute mtr_attr =
                db.create_attribute("spot.metrics",        CALI_TYPE_STRING, CALI_ATTR_GLOBAL);
            Attribute fmt_attr =
                db.create_attribute("spot.format.version", CALI_TYPE_INT,    CALI_ATTR_GLOBAL);

            // set the spot.metrics value
            db.set_global(mtr_attr, Variant(spot_metrics.c_str()));
            db.set_global(fmt_attr, Variant(spot_format_version));

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

            if (profilecfg & IoBytes || profilecfg & IoBandwidth)
                config()["CALI_SERVICES_ENABLE"].append(",io");
            if (profilecfg & WrapMpi) {
                config()["CALI_SERVICES_ENABLE"].append(",mpi");
                config()["CALI_MPI_BLACKLIST"     ] =
                    "MPI_Comm_rank,MPI_Comm_size,MPI_Wtick,MPI_Wtime";
            }
            if (profilecfg & WrapCuda)
                config()["CALI_SERVICES_ENABLE"].append(",cupti");
        }

    ~SpotController()
        { }
};

const char* spot_args[] = {
    "output",
    "aggregate_across_ranks",
    "profile",
    "mem.highwatermark",
    "io.bytes",
    "io.bandwidth",
    "profile.mpi",
    "profile.cuda",
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

    std::vector<std::string> deprecatedargs;

    it = args.find("profile");
    if (it != args.end())
        deprecatedargs = StringConverter(it->second).to_stringlist();

    const struct profile_cfg_info_t {
        const char* name; const char* oldname; ProfileConfig flag;
    } profile_cfg_info[] = {
        { "mem.highwatermark", "mem.highwatermark", MemHighWaterMark },
        { "io.bytes",          "io.bytes",          IoBytes          },
        { "io.bandwidth",      "io.bandwidth",      IoBandwidth      },
        { "profile.mpi",       "mpi",               WrapMpi          },
        { "profile.cuda",      "cuda",              WrapCuda         }
    };

    for (const profile_cfg_info_t& pinfo : profile_cfg_info) {
        auto it =
            args.find(pinfo.name);
        auto dit =
            std::find(deprecatedargs.begin(), deprecatedargs.end(), std::string(pinfo.oldname));

        if ((it != args.end() && StringConverter(it->second).to_bool()) || dit != deprecatedargs.end())
            profilecfg |= pinfo.flag;
    }

    return new SpotController(use_mpi, profilecfg, output.c_str());
}

std::string
check_args(const cali::ConfigManager::argmap_t& orig_args) {
    auto args = orig_args;

    {
        // Check the deprecated "profile=" argument
        auto it = args.find("profile");

        if (it != args.end()) {
            for (std::string& s : StringConverter(it->second).to_stringlist())
                if (s == "mem.highwatermark")
                    args["mem.highwatermark"] = "true";
                else if (s == "mpi")
                    args["profile.mpi"] = "true";
                else if (s == "cuda")
                    args["profile.cuda"] = "true";
                else
                    return std::string("spot: Unknown \"profile=\" option \"") + it->second + "\"";
        }
    }

    //
    //   Check if the required services for all requested profiling options
    // are there
    //

    const struct opt_info_t {
        const char* option;
        const char* service;
    } opt_info_list[] = {
        { "mem.highwatermark", "sysalloc" },
        { "io.bytes",          "io"       },
        { "io.bandwidth",      "io"       },
        { "profile.mpi",       "mpi"      },
        { "profile.cuda",      "cupti"    }
    };

    Services::add_default_services();
    auto svcs = Services::get_available_services();

    for (const opt_info_t o : opt_info_list) {
        auto it = args.find(o.option);

        if (it != args.end()) {
            bool ok = false;

            if (StringConverter(it->second).to_bool(&ok) == true)
                if (std::find(svcs.begin(), svcs.end(), o.service) == svcs.end())
                    return std::string("spot: ")
                        + o.service
                        + std::string(" service required for ")
                        + o.option
                        + std::string(" option is not available");

            if (!ok) // parse error
                return std::string("spot: Invalid value \"")
                    + it->second + "\" for "
                    + it->first;
        }
    }

    return "";
}

const char* docstr =
    "spot"
    "\n Record a time profile for the Spot visualization framework."
    "\n  Parameters:"
    "\n   output=filename|stdout|stderr:     Output location. Default: an auto-generated .cali file"
    "\n   aggregate_across_ranks=true|false: Aggregate results across MPI ranks"
    "\n   mem.highwatermark=true|false:      Record memory high-watermark for regions"
    "\n   profile.mpi=true|false:            Profile MPI functions"
    "\n   profile.cuda=true|false:           Profile CUDA API functions (e.g., cudaMalloc)"
    "\n   io.bytes=true|false:               Record I/O bytes written and read"
    "\n   io.bandwidth=true|false:           Record I/O bandwidth";

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo spot_controller_info { "spot", ::docstr, ::spot_args, ::make_spot_controller, ::check_args };

}
