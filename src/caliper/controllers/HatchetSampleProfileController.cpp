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
    Threads   = 1,
    Callpath  = 2,
    Module    = 4,
    SourceLoc = 8
};

class HatchetSampleProfileController : public cali::ChannelController
{
public:

    HatchetSampleProfileController(const std::string& output_, const std::string& format, const std::string& freq, int profile)
        : ChannelController("hatchet-sample-profile", 0, {
                { "CALI_CHANNEL_FLUSH_ON_EXIT", "false" },
                { "CALI_SERVICES_ENABLE",       "sampler,trace" },
            })
        {
            config()["CALI_SAMPLER_FREQUENCY"] = freq;

            std::string select  = "*,count()";
            std::string groupby = "prop:nested";

            bool use_symbollookup = false;

            if (profile & Threads)
                config()["CALI_SERVICES_ENABLE"     ].append(",pthread");

            if (profile & Callpath) {
                config()["CALI_SERVICES_ENABLE"     ].append(",callpath");
                config()["CALI_CALLPATH_SKIP_FRAMES"] = "4";

                groupby += ",source.function#callpath.address";
                use_symbollookup = true;
            }

            if (profile & Module) {
                config()["CALI_SYMBOLLOOKUP_LOOKUP_MODULE"] = "true";
                groupby += ",module#cali.sampler.pc";
                use_symbollookup = true;
            }

            if (profile & SourceLoc) {
                config()["CALI_SYMBOLLOOKUP_LOOKUP_SOURCELOC"] = "true";
                groupby += ",sourceloc#cali.sampler.pc";
                use_symbollookup = true;
            } else {
                config()["CALI_SYMBOLLOOKUP_LOOKUP_SOURCELOC"] = "false";
            }

            if (use_symbollookup)
                config()["CALI_SERVICES_ENABLE"].append(",symbollookup");

            std::string output(output_);

            if (output != "stdout" && output != "stderr") {
                auto pos = output.find_last_of('.');
                std::string ext = (format == "cali" ? ".cali" : ".json");

                if (pos == std::string::npos || output.substr(pos) != ext)
                    output.append(ext);
            }

            // make sure default services are loaded
            Services::add_default_services();

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
    const std::vector< std::tuple<const char*, ProfileConfig> > profinfo {
        { std::make_tuple( "sample.threads",   Threads   ) },
        { std::make_tuple( "sample.callpath",  Callpath  ) },
        { std::make_tuple( "lookup.module",    Module    ) },
        { std::make_tuple( "lookup.sourceloc", SourceLoc ) }
    };

    int profile = 7;

    for (auto tpl : profinfo) {
        auto it = args.find(std::get<0>(tpl));

        if (it != args.end()) {
            if (StringConverter(it->second).to_bool() == true)
                profile |= std::get<1>(tpl);
            else
                profile &= ~std::get<1>(tpl);
        }
    }

    return profile;
}

std::string
check_args(const cali::ConfigManager::argmap_t& args) {
    Services::add_default_services();
    auto svcs = Services::get_available_services();

    //
    // Check if the sampler service is there
    //

    if (std::find(svcs.begin(), svcs.end(), "sampler") == svcs.end())
        return "hatchet-sample-profile: sampler service is not available";

    //
    //   Check if the required services for all requested profiling options
    // are there
    //

    const struct opt_info_t {
        const char* option; const char* service; bool default_setting;
    } opt_info_list[] = {
        { "sample.threads"   , "pthread"      , true  },
        { "sample.callpath"  , "callpath"     , true  },
        { "sample.callpath"  , "symbollookup" , true  },
        { "lookup.module"    , "symbollookup" , true  },
        { "lookup.sourceloc" , "symbollookup" , false }
    };

    for (const opt_info_t o : opt_info_list) {
        bool is_on = o.default_setting;
        auto it = args.find(o.option);

        if (it != args.end()) {
            bool ok = false;
            is_on = (StringConverter(it->second).to_bool(&ok) == true);

            if (!ok) // parse error
                return std::string("hatchet-sample-profile: Invalid value \"")
                    + it->second + "\" for "
                    + it->first;
        }

        if (is_on && std::find(svcs.begin(), svcs.end(), o.service) == svcs.end())
            return std::string("hatchet-sample-profile: ")
                + o.service
                + std::string(" service required for ")
                + o.option
                + std::string(" option is not available");
    }

    //
    // Check if output.format is valid
    //

    {
        auto it = args.find("output.format");
        std::string format = (it == args.end() ? "json-split" : it->second);

        std::set<std::string> allowed_formats = { "cali", "json", "json-split" };

        if (allowed_formats.find(format) == allowed_formats.end())
            return std::string("hatchet-sample-profile: Invalid output format \"") + format + "\"";
    }

    return "";
}

cali::ChannelController*
make_controller(const cali::ConfigManager::argmap_t& args)
{
    auto it = args.find("output");
    std::string output = (it == args.end() ? "sample_profile" : it->second);

    it = args.find("output.format");
    std::string format = (it == args.end() ? "json-split" : it->second);

    if (format == "hatchet")
        format = "json-split";

    if (!(format == "json-split" || format == "json" || format == "cali")) {
        format = "json-split";
        Log(0).stream() << "hatchet-sample-profile: Unknown output format \"" << format
                        << "\". Using json-split."
                        << std::endl;
    }

    it = args.find("sample.frequency");
    std::string freq = (it == args.end()) ? "200" : it->second;

    return new HatchetSampleProfileController(output, format, freq, get_profile_cfg(args));
}

const char* controller_args[] = {
    "output",
    "output.format",
    "sample.frequency",
    "sample.threads",
    "sample.callpath",
    "lookup.module",
    "lookup.sourceloc",
    nullptr
};

const char* docstr =
    "hatchet-sample-profile"
    "\n Record a sampling profile for processing with hatchet."
    "\n  Parameters:"
    "\n   output=filename|stdout|stderr:     Output location. Default: stderr"
    "\n   output.format=hatchet|cali|json:   Output format. Default hatchet (json-split)"
    "\n   sample.frequency=<N>:              Sampling frequency in Hz. Default: 200"
    "\n   sample.threads=true|false:         Profile all threads. Default: true"
    "\n   sample.callpath=true|false:        Perform call-stack unwinding. Default: true"
    "\n   lookup.module=true|false:          Lookup source module (.so/exe)"
    "\n   lookup.sourceloc=true|false:       Lookup source location (file+line).";

} // namespace [anonymous]

namespace cali
{

ConfigManager::ConfigInfo hatchet_sample_profile_controller_info
{
    "hatchet-sample-profile", ::docstr, ::controller_args, ::make_controller, ::check_args
};

}
