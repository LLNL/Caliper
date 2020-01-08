// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

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

// enum ProfileConfig {
//     WrapMpi       = 1,
//     WrapCuda      = 2,
//     MemHighWaterMark = 4,
//     IoBytes       = 8,
//     IoBandwidth   = 16,
//     CalcInclusive = 32
// };

class RuntimeReportController : public cali::ChannelController
{
public:

    RuntimeReportController(bool use_mpi, const ConfigManager::Options& opts)
        : ChannelController("runtime-report", 0, {
                { "CALI_CHANNEL_FLUSH_ON_EXIT",      "false" },
                { "CALI_EVENT_ENABLE_SNAPSHOT_INFO", "false" },
                { "CALI_TIMER_SNAPSHOT_DURATION",    "true"  },
                { "CALI_TIMER_INCLUSIVE_DURATION",   "false" },
                { "CALI_TIMER_UNIT", "sec" }
            })
        {
            // Config for first aggregation step in MPI mode (process-local aggregation)
            std::string local_select =
                " sum(sum#time.duration)";
            // Config for serial-mode aggregation
            std::string serial_select =
                " inclusive_sum(sum#time.duration) as \"Inclusive time\""
                ",sum(sum#time.duration) as \"Exclusive time\""
                ",percent_total(sum#time.duration) as \"Time %\"";

            std::string tmetric = "sum#sum#time.duration";

            if (opts.is_enabled("calc.inclusive")) {
                local_select += ",inclusive_sum(sum#time.duration)";
                tmetric = "inclusive#sum#time.duration";
            }

            // Config for second aggregation step in MPI mode (cross-process aggregation)
            std::string cross_select =
                  std::string(" min(") + tmetric + ") as \"Min time/rank\""
                + std::string(",max(") + tmetric + ") as \"Max time/rank\""
                + std::string(",avg(") + tmetric + ") as \"Avg time/rank\""
                + std::string(",percent_total(sum#sum#time.duration) as \"Time %\"");

            // if (profile & MemHighWaterMark) {
            //     local_select += ",max(max#alloc.region.highwatermark)";
            //     cross_select +=
            //         ",max(max#max#alloc.region.highwatermark) as \"Max Alloc'd Mem\"";
            //     serial_select +=
            //         ",max(max#alloc.region.highwatermark) as \"Max Alloc'd Mem\"";

            //     config()["CALI_SERVICES_ENABLE"   ].append(",alloc,sysalloc");
            //     config()["CALI_ALLOC_RECORD_HIGHWATERMARK"] = "true";
            //     config()["CALI_ALLOC_TRACK_ALLOCATIONS"   ] = "false";
            // }
            // if (profile & WrapMpi) {
            //     config()["CALI_SERVICES_ENABLE"   ].append(",mpi");
            //     config()["CALI_MPI_BLACKLIST"     ] =
            //         "MPI_Comm_rank,MPI_Comm_size,MPI_Wtick,MPI_Wtime";
            // }
            // if (profile & WrapCuda) {
            //     config()["CALI_SERVICES_ENABLE"   ].append(",cupti");
            // }
            // if (profile & IoBytes || profile & IoBandwidth) {
            //     config()["CALI_SERVICES_ENABLE"   ].append(",io");
            //     local_select +=
            //         ",sum(sum#io.bytes.written),sum(sum#io.bytes.read)";
            // }
            // if (profile & IoBytes) {
            //     cross_select +=
            //         ",avg(sum#sum#io.bytes.written) as \"Avg written\""
            //         ",avg(sum#sum#io.bytes.read) as \"Avg read\""
            //         ",sum(sum#sum#io.bytes.written) as \"Total written\""
            //         ",sum(sum#sum#io.bytes.read) as \"Total read\"";
            //     serial_select +=
            //         ",sum(sum#io.bytes.written) as \"Total written\""
            //         ",sum(sum#io.bytes.read) as \"Total read\"";
            // }
            // if (profile & IoBandwidth) {
            //     groupby += ",io.region";

            //     serial_select +=
            //         ",io.region as I/O"
            //         ",ratio(sum#io.bytes.written,sum#time.duration,8e-6) as \"Write Mbit/s\""
            //         ",ratio(sum#io.bytes.read,sum#time.duration,8e-6) as \"Read Mbit/s\"";
            //     cross_select +=
            //         ",io.region as I/O"
            //         ",ratio(sum#sum#io.bytes.written,sum#sum#time.duration,8e-6) as \"Write Mbit/s\""
            //         ",ratio(sum#sum#io.bytes.read,sum#sum#time.duration,8e-6) as \"Read Mbit/s\"";
            // }

            if (use_mpi) {
                config()["CALI_SERVICES_ENABLE"   ] = opts.services("aggregate,event,mpireport,timestamp");
                config()["CALI_MPIREPORT_FILENAME"] = opts.get("output", "stderr").to_string();
                config()["CALI_MPIREPORT_WRITE_ON_FINALIZE"] = "false";
                config()["CALI_MPIREPORT_LOCAL_CONFIG"] =
                    std::string("select ")
                    + opts.query_select("local", local_select)
                    + " group by "
                    + opts.query_groupby("local", "prop:nested");
                config()["CALI_MPIREPORT_CONFIG"  ] =
                    std::string("select ")
                    + opts.query_select("cross", cross_select)
                    + " group by "
                    + opts.query_groupby("cross", "prop:nested")
                    + " format tree";
            } else {
                config()["CALI_SERVICES_ENABLE"   ] = opts.services("aggregate,event,report,timestamp");
                config()["CALI_REPORT_FILENAME"   ] = opts.get("output", "stderr").to_string();
                config()["CALI_REPORT_CONFIG"     ] =
                    std::string("select ")
                    + opts.query_select("serial", serial_select)
                    + " group by "
                    + opts.query_groupby("serial", "prop:nested")
                    + " format tree";
            }

            opts.append_extra_config_flags(config());
        }
};


// Parse the "mpi=" argument
bool
use_mpi(const cali::ConfigManager::Options& opts)
{
    auto services = Services::get_available_services();

    bool have_mpireport =
        std::find(services.begin(), services.end(), "mpireport") != services.end();

    bool use_mpi = opts.is_enabled("aggregate_across_ranks");

    if (use_mpi && !have_mpireport) {
        use_mpi = false;
        Log(0).stream() << "runtime-report: cannot enable mpi support: mpireport service is not available."
                        << std::endl;
    }

    return use_mpi;
}

// // Parse the "profile=" argument
// int
// get_profile_cfg(const cali::ConfigManager::argmap_t& args)
// {
//     //   Parse the "profile" argument. This is deprecated, should eventually
//     // be replaced with the individual profiling opts ("profile.mpi" etc.)

//     std::vector<std::string> profile_opts;

//     {
//         auto argit = args.find("profile");

//         if (argit != args.end())
//             profile_opts = StringConverter(argit->second).to_stringlist(",:");
//     }

//     // make sure default services are loaded
//     Services::add_default_services();
//     auto srvcs = Services::get_available_services();

//     const std::vector< std::tuple<const char*, ProfileConfig, const char*> > profinfo {
//         { std::make_tuple( "mpi",            WrapMpi,       "mpi")       },
//         { std::make_tuple( "cuda",           WrapCuda,      "cupti" )    },
//         { std::make_tuple( "mem.highwatermark", MemHighWaterMark, "sysalloc" ) },
//         { std::make_tuple( "io.bytes",       IoBytes,       "io" ) },
//         { std::make_tuple( "io.bandwidth",   IoBandwidth,   "io" ) },
//         { std::make_tuple( "calc.inclusive", CalcInclusive, "mpireport" ) }
//     };

//     int profile = 0;

//     {
//         const struct optmap_t {
//             const char* newname; const char* oldname;
//         } optmap[] = {
//             { "profile.mpi",  "mpi"  },
//             { "profile.cuda", "cuda" },
//             { "mem.highwatermark", "mem.highwatermark" },
//             { "io.bytes",     "io.bytes" },
//             { "io.bandwidth", "io.bandwidth" },
//             { "calc.inclusive", "calc.inclusive" }
//         };

//         for (const auto &entry : optmap) {
//             auto it = args.find(entry.newname);
//             if (it != args.end())
//                 if (StringConverter(it->second).to_bool())
//                     profile_opts.push_back(entry.oldname);
//         }
//     }

//     for (const std::string& s : profile_opts) {
//         auto it = std::find_if(profinfo.begin(), profinfo.end(), [s](decltype(profinfo.front()) tpl){
//                 return s == std::get<0>(tpl);
//             });

//         if (it == profinfo.end())
//             Log(0).stream() << "runtime-report: Unknown profile option \"" << s << "\"" << std::endl;
//         else {
//             if (std::find(srvcs.begin(), srvcs.end(), std::get<2>(*it)) == srvcs.end())
//                 Log(0).stream() << "runtime-report: cannot enable " << std::get<0>(*it)
//                               << " profiling: " << std::get<2>(*it)
//                               << " service is not available."
//                               << std::endl;
//             else
//                 profile |= std::get<1>(*it);
//         }
//     }

//     return profile;
// }

cali::ChannelController*
make_runtime_report_controller(const cali::ConfigManager::Options& opts)
{
    return new RuntimeReportController(use_mpi(opts), opts);
}

// std::string
// check_args(const cali::ConfigManager::Options& opts) {
//     auto args = orig_args;

//     {
//         // Check the deprecated "profile=" argument
//         auto it = args.find("profile");

//         if (it != args.end()) {
//             const struct profile_info_t {
//                 const char* oldopt; const char* newopt;
//             } profile_info_list[] = {
//                 { "mem.highwatermark", "mem.highwatermark" },
//                 { "io.bytes",          "io.bytes"          },
//                 { "io.bandwidth",      "io.bandwidth"      },
//                 { "mpi",               "profile.mpi"       },
//                 { "cuda",              "profile.cuda"      }
//             };

//             for (const std::string& opt : StringConverter(it->second).to_stringlist(",:")) {
//                 auto p = std::find_if(std::begin(profile_info_list), std::end(profile_info_list),
//                             [opt](const profile_info_t& pp){
//                                 return opt == pp.oldopt;
//                             });

//                 if (p == std::end(profile_info_list))
//                     return std::string("runtime-report: unknown \"profile=\" option \"") + opt + "\"";
//                 else
//                     args[p->newopt] = "true";
//             }
//         }
//     }

//     //
//     //   Check if the required services for all requested profiling options
//     // are there
//     //

//     const struct opt_info_t {
//         const char* option;
//         const char* service;
//     } opt_info_list[] = {
//         { "mem.highwatermark",      "sysalloc"  },
//         { "io.bytes",               "io"        },
//         { "io.bandwidth",           "io"        },
//         { "profile.cuda",           "cupti"     },
//         { "profile.mpi",            "mpi"       },
//         { "aggregate_across_ranks", "mpireport" }
//     };

//     Services::add_default_services();
//     auto svcs = Services::get_available_services();

//     for (const opt_info_t o : opt_info_list) {
//         auto it = args.find(o.option);

//         if (it != args.end()) {
//             bool ok = false;

//             if (StringConverter(it->second).to_bool(&ok) == true)
//                 if (std::find(svcs.begin(), svcs.end(), o.service) == svcs.end())
//                     return std::string("runtime-report: ")
//                         + o.service
//                         + std::string(" service required for ")
//                         + o.option
//                         + std::string(" option is not available");

//             if (!ok) // parse error
//                 return std::string("runtime-report: Invalid value \"")
//                     + it->second + "\" for "
//                     + it->first;
//         }
//     }

//     return "";
// }

const char* runtime_report_categories[] = {
    "metric",
    "output",
    "region",
    nullptr
};

const char* runtime_report_options =
    "{"
    " \"name\": \"calc.inclusive\","
    " \"type\": \"bool\","
    " \"description\": \"Report inclusive instead of exclusive times\""
    "},"
    "{"
    " \"name\": \"aggregate_across_ranks\","
    " \"type\": \"bool\","
    " \"description\": \"Aggregate results across MPI ranks\""
    "}";

const char* docstr =
    "Print a time profile for annotated regions";

// const char* runtime_report_args[] = {
//     "output",
//     "mpi",
//     "aggregate_across_ranks",
//     "profile",
//     "mem.highwatermark",
//     "profile.mpi",
//     "profile.cuda",
//     "io.bytes",
//     "io.bandwidth",
//     "calc.inclusive",
//     nullptr
// };

// const char* docstr =
//     "runtime-report"
//     "\n Print a time profile for annotated regions."
//     "\n  Parameters:"
//     "\n   output=filename|stdout|stderr:     Output location. Default: stderr"
//     "\n   aggregate_across_ranks=true|false: Aggregate results across MPI ranks"
//     "\n   profile.mpi=true|false:            Profile MPI functions"
//     "\n   profile.cuda=true|false:           Profile CUDA API functions (e.g., cudaMalloc)"
//     "\n   mem.highwatermark=true|false:      Record memory high-watermark for regions"
//     "\n   io.bytes=true|false:               Record I/O bytes written and read"
//     "\n   io.bandwidth=true|false:           Record I/O bandwidth"
//     "\n   calc.inclusive=true|false:         Report inclusive instead of exclusive times";

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo runtime_report_controller_info
{
    "runtime-report",
    ::docstr,
    ::runtime_report_options,
    ::runtime_report_categories,
    ::make_runtime_report_controller,
    nullptr
};

}
