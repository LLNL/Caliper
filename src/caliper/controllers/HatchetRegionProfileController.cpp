// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

#include "caliper/common/Log.h"
#include "caliper/common/StringConverter.h"

#include "../../services/Services.h"

#include <algorithm>
#include <set>
#include <tuple>

using namespace cali;

namespace
{

enum ProfileConfig {
    WrapMpi     = 1,
    WrapCuda    = 2,
    MallocInfo  = 4,
    MemHighWaterMark = 8,
    IoBytes     = 16,
    IoBandwidth = 32
};

class HatchetRegionProfileController : public cali::ChannelController
{
public:

    HatchetRegionProfileController(const std::string& output_, const std::string& format, int profile)
        : ChannelController("hatchet-region-profile", 0, {
                { "CALI_CHANNEL_FLUSH_ON_EXIT",      "false" },
                { "CALI_SERVICES_ENABLE", "aggregate,event,timestamp" },
                { "CALI_EVENT_ENABLE_SNAPSHOT_INFO", "false" },
                { "CALI_TIMER_SNAPSHOT_DURATION",    "true"  },
                { "CALI_TIMER_INCLUSIVE_DURATION",   "false" },
                { "CALI_AGGREGATE_KEY",              "annotation,function,loop" },
                { "CALI_TIMER_UNIT", "sec" }
            })
        {
            std::string select  = "*,sum(sum#time.duration) as time";
            std::string groupby = "prop:nested";

            if (profile & WrapMpi) {
                config()["CALI_MPI_BLACKLIST"     ] =
                    "MPI_Comm_rank,MPI_Comm_size,MPI_Wtick,MPI_Wtime";
            }
            if (profile & WrapCuda) {
                config()["CALI_AGGREGATE_KEY"     ].append(",cupti.runtimeAPI");
                config()["CALI_SERVICES_ENABLE"   ].append(",cupti");
            }
            if (profile & IoBytes) {
                config()["CALI_SERVICES_ENABLE"   ].append(",io");
                select += ",sum(sum#io.bytes.written),sum(sum#io.bytes.read)";
            }

            std::string output(output_);

            if (output != "stdout" && output != "stderr") {
                auto pos = output.find_last_of('.');
                std::string ext = (format == "cali" ? ".cali" : ".json");

                if (pos == std::string::npos || output.substr(pos) != ext)
                    output.append(ext);
            }

            auto services = Services::get_available_services();
            bool have_mpi =
                std::find(services.begin(), services.end(), "mpireport")    != services.end();
            bool have_adiak =
                std::find(services.begin(), services.end(), "adiak_import") != services.end();

            if (have_adiak)
                config()["CALI_SERVICES_ENABLE"   ].append(",adiak_import");

            if (have_mpi) {
                groupby += ",mpi.rank";

                config()["CALI_SERVICES_ENABLE"   ].append(",mpi,mpireport");
                config()["CALI_AGGREGATE_KEY"     ].append(",mpi.function,mpi.rank");
                config()["CALI_MPIREPORT_FILENAME"] = output;
                config()["CALI_MPIREPORT_WRITE_ON_FINALIZE"] = "false";
                config()["CALI_MPIREPORT_CONFIG"  ] =
                    std::string("select ") + select + " group by " + groupby + " format " + format;
            } else {
                config()["CALI_SERVICES_ENABLE"   ].append(",report");
                config()["CALI_REPORT_FILENAME"   ] = output;
                config()["CALI_REPORT_CONFIG"     ] =
                    std::string("select ") + select + " group by " + groupby + " format " + format;
            }
        }
};

int
get_profile_cfg(const cali::ConfigManager::argmap_t& args)
{
    // make sure default services are loaded
    Services::add_default_services();
    auto srvcs = Services::get_available_services();

    const std::vector< std::tuple<const char*, ProfileConfig, const char*> > profinfo {
        { std::make_tuple( "profile.mpi",  WrapMpi,     "mpi")   },
        { std::make_tuple( "profile.cuda", WrapCuda,    "cupti") },
        { std::make_tuple( "io.bytes",     IoBytes,     "io" )   }
    };

    int profile = 0;

    for (auto tpl : profinfo) {
        auto it = args.find(std::get<0>(tpl));

        if (it == args.end() || StringConverter(it->second).to_bool() == false)
            continue;

        if (std::find(srvcs.begin(), srvcs.end(), std::get<2>(tpl)) == srvcs.end())
            Log(0).stream() << "hatchet-region-profile: cannot enable " << std::get<0>(tpl)
                            << " profiling: " << std::get<2>(tpl)
                            << " service is not available."
                            << std::endl;
        else
            profile |= std::get<1>(tpl);
    }

    return profile;
}

std::string
check_args(const cali::ConfigManager::argmap_t& args) {
    //
    //   Check if the required services for all requested profiling options
    // are there
    //

    const struct opt_info_t {
        const char* option; const char* service;
    } opt_info_list[] = {
        { "io.bytes"     , "io"    },
        { "profile.cuda" , "cupti" },
        { "profile.mpi"  , "mpi"   }
    };

    Services::add_default_services();
    auto svcs = Services::get_available_services();

    for (const opt_info_t o : opt_info_list) {
        auto it = args.find(o.option);

        if (it != args.end()) {
            bool ok = false;

            if (StringConverter(it->second).to_bool(&ok) == true) 
                if (std::find(svcs.begin(), svcs.end(), o.service) == svcs.end())
                    return std::string("hatchet-region-profile: ") 
                        + o.service
                        + std::string(" service required for ")
                        + o.option
                        + std::string(" option is not available");

            if (!ok) // parse error
                return std::string("hatchet-region-profile: Invalid value \"") 
                    + it->second + "\" for " 
                    + it->first;
        }
    }

    {
        // Check if output.format is valid

        auto it = args.find("output.format");
        std::string format = (it == args.end() ? "json-split" : it->second);

        std::set<std::string> allowed_formats = { "cali", "json", "json-split" };

        if (allowed_formats.find(format) == allowed_formats.end())
            return std::string("hatchet-region-profile: Invalid output format \"") + format + "\"";
    }

    return "";
}

cali::ChannelController*
make_controller(const cali::ConfigManager::argmap_t& args)
{
    auto it = args.find("output");
    std::string output = (it == args.end() ? "region_profile" : it->second);

    it = args.find("output.format");
    std::string format = (it == args.end() ? "json-split" : it->second);

    if (format == "hatchet")
        format = "json-split";

    if (!(format == "json-split" || format == "json" || format == "cali")) {
        format = "json-split";
        Log(0).stream() << "hatchet-region-profile: Unknown output format \"" << format
                        << "\". Using json-split."
                        << std::endl;
    }

    return new HatchetRegionProfileController(output, format, get_profile_cfg(args));
}

const char* controller_args[] = {
    "output",
    "output.format",
    "profile.mpi",
    "profile.cuda",
    "io.bytes",
    nullptr
};

const char* docstr =
    "hatchet-region-profile"
    "\n Record a region time profile for processing with hatchet."
    "\n  Parameters:"
    "\n   output=filename|stdout|stderr:     Output location. Default: stderr"
    "\n   output.format=hatchet|cali|json:   Output format. Default hatchet (json-split)"
    "\n   profile.mpi=true|false:            Profile MPI functions"
    "\n   profile.cuda=true|false:           Profile CUDA API functions (e.g., cudaMalloc)"
    "\n   io.bytes=true|false:               Record I/O bytes written and read";

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo hatchet_region_profile_controller_info
{
    "hatchet-region-profile", ::docstr, ::controller_args, ::make_controller, ::check_args
};

}
