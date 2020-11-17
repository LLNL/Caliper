// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "caliper/caliper-config.h"

#include "caliper/Caliper.h"
#include "caliper/ConfigManager.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include "../services/Services.h"

#include <algorithm>
#include <cstdlib>

using namespace cali;


namespace
{

Channel* make_flush_trigger_channel(Caliper* c)
{
    auto services =
        services::get_available_services();
    bool have_mpiflush =
        std::find(services.begin(), services.end(), "mpiflush") != services.end();

    std::map<std::string, std::string> cfgmap = {
            { "CALI_CHANNEL_CONFIG_CHECK", "false" }
        };

    if (have_mpiflush)
        cfgmap["CALI_SERVICES_ENABLE"] = "mpi,mpiflush";

    RuntimeConfig cfg;
    cfg.allow_read_env(false);
    cfg.import(cfgmap);

    return c->create_channel("builtin.configmgr", cfg);
}

}

namespace cali
{

namespace internal
{

/// \brief Create and configure the builtin ConfigManager, if needed
void init_builtin_configmanager(Caliper* c)
{
    const char* configstr = std::getenv("CALI_CONFIG");

    if (!configstr)
        return;

    ConfigManager mgr;
    mgr.add(configstr);

    if (mgr.error()) {
        Log(0).stream() << "CALI_CONFIG: error: " << mgr.error_msg()
                        << std::endl;
        return;
    }

    //   Make a channel to trigger the ConfigManager flush. Use the mpiflush
    // service if it is available to trigger flushes at MPI_Finalize(). In
    // that case, set the configmgr.flushed attribute to skip flushing at the
    // end of the program.

    Attribute flag_attr =
        c->create_attribute("cali.configmgr.flushed", CALI_TYPE_BOOL,
                            CALI_ATTR_SKIP_EVENTS |
                            CALI_ATTR_HIDDEN      |
                            CALI_ATTR_ASVALUE);

    Channel* channel = ::make_flush_trigger_channel(c);

    mgr.start();

    channel->events().write_output_evt.connect(
        [mgr,flag_attr](Caliper* c, Channel* channel, const SnapshotRecord*) mutable {
            if (c->get(channel, flag_attr).value().to_bool() == true)
                return;

            mgr.flush();

            c->set(channel, flag_attr, Variant(true));
        });

    Log(1).stream() << "Registered builtin ConfigManager" << std::endl;
}

} // namespace cali::internal

} // namespace cali
