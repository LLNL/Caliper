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

    SpotV1Controller(const cali::ConfigManager::argmap_t& args)
        : cali::ChannelController("spot-v1", 0, {
                { "CALI_SERVICES_ENABLE", "event,aggregate,spot,timestamp" },
                { "CALI_CHANNEL_FLUSH_ON_EXIT",   "false"        },
                { "CALI_TIMER_SNAPSHOT_DURATION", "false"        },
                { "CALI_SPOT_TIME_DIVISOR",       "1000"         },
                { "CALI_SPOT_Y_AXES",             "Milliseconds" }
            })
        {
            auto it = args.find("config");
            if (it != args.end())
                config()["CALI_SPOT_CONFIG"] = it->second;

            it = args.find("code_version");
            if (it != args.end())
                config()["CALI_SPOT_CODE_VERSION"] = it->second;

            it = args.find("title");
            if (it != args.end())
                config()["CALI_SPOT_TITLE"] = it->second;
        }

    void flush() {
        int rank = 0;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        if (rank == 0 && channel())
            cali_channel_flush(channel()->id(), CALI_FLUSH_CLEAR_BUFFERS);
    }
};

const char* spot_v1_args[] = { "config", "code_version", "title", nullptr };

cali::ChannelController*
make_spot_v1_controller(const cali::ConfigManager::argmap_t& args)
{
    return new SpotV1Controller(args);
}

} // namespace [anonymous]

namespace cali
{

cali::ConfigManager::ConfigInfo spot_v1_controller_info { "spot-v1", ::spot_v1_args, ::make_spot_v1_controller };

}
