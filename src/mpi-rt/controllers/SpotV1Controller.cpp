// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/Caliper.h"
#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"

#include "caliper/cali.h"

#include <mpi.h>

namespace
{

class SpotV1Controller : public cali::ChannelController
{
public:

    SpotV1Controller(const char* name, const cali::config_map_t& initial_cfg, const cali::ConfigManager::Options& opts)
        : cali::ChannelController(name, 0, initial_cfg)
        {
            if (opts.is_set("config"))
                config()["CALI_SPOT_CONFIG"] = opts.get("config", "").to_string();
            if (opts.is_set("code_version"))
                config()["CALI_SPOT_CODE_VERSION"] = opts.get("code_version", "").to_string();
            if (opts.is_set("title"))
                config()["CALI_SPOT_TITLE"] = opts.get("title", "").to_string();

            opts.update_channel_config(config());
        }

    void flush() {
        int rank = 0;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        if (rank == 0 && channel())
            cali_channel_flush(channel()->id(), CALI_FLUSH_CLEAR_BUFFERS);
    }
};

const char* controller_spec =
    "{"
    " \"name\"        : \"spot-v1\","
    " \"description\" : \"Write Spot v1 JSON output\","
    " \"services\"    : [\"event\", \"aggregate\", \"spot\", \"timestamp\" ],"
    " \"config\"      : "
    "   { \"CALI_CHANNEL_FLUSH_ON_EXIT\"      : \"false\","
    "     \"CALI_SPOT_TIME_DIVISOR\"          : \"1000\","
    "     \"CALI_SPOT_Y_AXES\"                : \"Milliseconds\""
    "   },"
    " \"options\": "
    " ["
    "  {"
    "   \"name\": \"config\","
    "   \"description\": \"Attribute:Filename pairs in which to dump Spot data\""
    "  },"
    "  {"
    "   \"name\": \"code_version\","
    "   \"description\": \"Version number (or git hash) to represent this run of the code\""
    "  },"
    "  {"
    "   \"name\": \"title\","
    "   \"description\": \"Title for this test\""
    "  }"
    " ]"
    "}";

cali::ChannelController*
make_spot_v1_controller(const char* name, const cali::config_map_t& initial_cfg, const cali::ConfigManager::Options& opts)
{
    return new SpotV1Controller(name, initial_cfg, opts);
}

} // namespace [anonymous]

namespace cali
{

cali::ConfigManager::ConfigInfo spot_v1_controller_info
{
    ::controller_spec, ::make_spot_v1_controller, nullptr
};

}
