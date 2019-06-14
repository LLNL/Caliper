#include "caliper/caliper-config.h"

#include <caliper/Caliper.h>
#include <caliper/ChannelController.h>
#include <caliper/ConfigManager.h>

#include <caliper/reader/Aggregator.h>
#include <caliper/reader/CaliperMetadataDB.h>
#include <caliper/reader/CalQLParser.h>
#include <caliper/reader/FormatProcessor.h>

#include <caliper/common/Log.h>
#include <caliper/common/OutputStream.h>

#include <unistd.h>

#include <ctime>

#include "caliper/cali-mpi.h"

#include <mpi.h>

using namespace cali;

namespace
{

std::string
make_filename()
{
    char   timestr[16];
    time_t tm = time(NULL);
    strftime(timestr, sizeof(timestr), "%y%m%d-%H%M%S", localtime(&tm));

    return std::string(timestr) + std::to_string(getpid()) + ".cali";
}

class SpotController : public cali::ChannelController
{
    std::string m_output;
    bool m_use_mpi;
    
public:
    
    void
    flush() {    
        Log(1).stream() << "[spot controller]: Flushing Caliper data" << std::endl;

        // --- Setup output reduction aggregator

        QuerySpec  output_spec =
            CalQLParser("select *,"
                        " min(inclusive#sum#time.duration)"
                        ",max(inclusive#sum#time.duration)"
                        ",avg(inclusive#sum#time.duration)"
                        " format cali").spec();

        Aggregator output_agg(output_spec);

        CaliperMetadataDB db;
        Caliper c;

        // ---   Flush Caliper buffers into intermediate aggregator to calculate
        //     inclusive times

        {
            QuerySpec  inclusive_spec =
                CalQLParser("aggregate"
                            " inclusive_sum(sum#time.duration)"
                            " group by prop:nested").spec();

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
            db.import_globals(c);

            std::string output = m_output;

            if (m_output.empty())
                output = ::make_filename();
        
            OutputStream    stream;
            stream.set_filename(output.c_str(), c, c.get_globals());

            FormatProcessor formatter(output_spec, stream);

            output_agg.flush(db, formatter);
            formatter.flush(db);
        }
    }
    
    SpotController(bool use_mpi, const char* output)
        : ChannelController("spot", 0, {
                { "CALI_SERVICES_ENABLE", "aggregate,env,event,timestamp" },
                { "CALI_EVENT_ENABLE_SNAPSHOT_INFO", "false" },
                { "CALI_TIMER_INCLUSIVE_DURATION", "false" },
                { "CALI_TIMER_SNAPSHOT_DURATION",  "true" },
                { "CALI_CHANNEL_FLUSH_ON_EXIT", "false" },
                { "CALI_CHANNEL_CONFIG_CHECK",  "false" }
            }),
          m_output(output),
          m_use_mpi(use_mpi)
        { }
    
    ~SpotController()
        { }
};

const char* spot_args[] = { "output", nullptr };

cali::ChannelController*
make_spot_controller(const cali::ConfigManager::argmap_t& args, bool use_mpi) {
    auto it = args.find("output");
    std::string output = (it == args.end() ? "" : it->second);

    return new SpotController(use_mpi, output.c_str());
}

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo spot_controller_info { "spot", ::spot_args, ::make_spot_controller };

}
