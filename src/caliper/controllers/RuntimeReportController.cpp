#include "caliper/caliper-config.h"

#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

#include "caliper/common/Log.h"
#include "caliper/common/StringConverter.h"

#include "../../services/Services.h"

#include <algorithm>
#include <tuple>

using namespace cali;

namespace
{

enum Wrapper {
    WrapMpi  = 1,
    WrapCuda = 2
};

class RuntimeReportController : public cali::ChannelController
{
public:

    RuntimeReportController(bool use_mpi, const char* output, int wrappers)
        : ChannelController("runtime-report", 0, {
                { "CALI_CHANNEL_FLUSH_ON_EXIT",      "false" },
                { "CALI_SERVICES_ENABLE", "aggregate,event,timestamp" },
                { "CALI_EVENT_ENABLE_SNAPSHOT_INFO", "false" },
                { "CALI_TIMER_SNAPSHOT_DURATION",    "true"  },
                { "CALI_TIMER_INCLUSIVE_DURATION",   "false" },
                { "CALI_TIMER_UNIT", "sec" }
            })
        {
            if (use_mpi) {
                config()["CALI_SERVICES_ENABLE"   ].append(",mpireport");
                config()["CALI_MPIREPORT_FILENAME"] = output;
                config()["CALI_MPIREPORT_WRITE_ON_FINALIZE"] = "false";
                config()["CALI_MPIREPORT_CONFIG"  ] =
                    "select min(sum#time.duration) as \"Min time/rank\""
                    ",max(sum#time.duration) as \"Max time/rank\""
                    ",avg(sum#time.duration) as \"Avg time/rank\""
                    ",percent_total(sum#time.duration) as \"Time % (total)\""
                    " group by prop:nested format tree";
            } else {
                config()["CALI_SERVICES_ENABLE"   ].append(",report");
                config()["CALI_REPORT_FILENAME"   ] = output;
                config()["CALI_REPORT_CONFIG"     ] =
                    "select inclusive_sum(sum#time.duration) as \"Inclusive time\""
                    ",sum(sum#time.duration) as \"Exclusive time\""
                    ",percent_total(sum#time.duration) as \"Time %\""
                    " group by prop:nested format tree";
            }

            if (wrappers & WrapMpi) {
                config()["CALI_SERVICES_ENABLE"   ].append(",mpi");
                config()["CALI_MPI_BLACKLIST"     ] =
                    "MPI_Comm_rank,MPI_Comm_size,MPI_Wtick,MPI_Wtime";
            }

            if (wrappers & WrapCuda) {
                config()["CALI_SERVICES_ENABLE"   ].append(",cupti");
            }
        }
};

const char* runtime_report_args[] = { "output", "mpi", "profile", nullptr };

// Parse the "mpi=" argument
bool
use_mpi(const cali::ConfigManager::argmap_t& args)
{
    auto services = Services::get_available_services();

    bool have_mpireport =
        std::find(services.begin(), services.end(), "mpireport") != services.end();

    bool use_mpi = have_mpireport;
    
    auto it = args.find("mpi");
    
    if (it != args.end())
        use_mpi = StringConverter(it->second).to_bool();

    if (use_mpi && !have_mpireport) {
        use_mpi = false;
        Log(0).stream() << "runtime-report: cannot enable mpi support: mpireport service is not available."
                        << std::endl;
    }

    return use_mpi;
}

// Parse the "profile=" argument
int
profile_cfg(const cali::ConfigManager::argmap_t& args)
{
    auto argit = args.find("profile");

    if (argit == args.end())
        return 0;

    int wrappers = 0;
    auto srvcs = Services::get_available_services();

    const std::vector< std::tuple<const char*, Wrapper, const char*> > wrapinfo {
        { std::make_tuple( "mpi",  WrapMpi,  "mpi")   },
        { std::make_tuple( "cuda", WrapCuda, "cupti") }
    };

    for (const std::string& s : StringConverter(argit->second).to_stringlist(",:")) {
        auto it = std::find_if(wrapinfo.begin(), wrapinfo.end(), [s](decltype(wrapinfo.front()) tpl){
                return s == std::get<0>(tpl);
            });

        if (it == wrapinfo.end())
            Log(0).stream() << "runtime-report: Unknown profile option \"" << s << "\"" << std::endl;
        else {
            if (std::find(srvcs.begin(), srvcs.end(), std::get<2>(*it)) == srvcs.end())
                Log(0).stream() << "runtime-report: cannot enable " << std::get<0>(*it)
                              << " profiling: " << std::get<2>(*it)
                              << " service is not available."
                              << std::endl;
            else
                wrappers |= std::get<1>(*it);
        }
    }

    return wrappers;
}

cali::ChannelController*
make_runtime_report_controller(const cali::ConfigManager::argmap_t& args)
{
    auto it = args.find("output");
    std::string output = (it == args.end() ? "stderr" : it->second);

    return new RuntimeReportController(use_mpi(args), output.c_str(), profile_cfg(args));
}

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo runtime_report_controller_info
{
    "runtime-report", "runtime-report(output=<filename>,mpi=true|false,profile=[mpi:cupti]): Print region time profile", ::runtime_report_args, ::make_runtime_report_controller
};

}
