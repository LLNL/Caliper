// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "caliper/caliper-config.h"

#include "caliper/Caliper.h"
#include "caliper/ConfigManager.h"

#include "caliper/common/Log.h"

#include <cstdlib>

using namespace cali;


namespace cali
{

namespace internal
{

/// \brief Create and configure the builtin ConfigManager, if needed
void init_builtin_configmanager(Caliper* c, Channel* channel)
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

    mgr.start();

    channel->events().write_output_evt.connect(
        [mgr](Caliper*,Channel*,const SnapshotRecord*) mutable {
            mgr.flush();
        });

    Log(1).stream() << channel->name() << ": Registered builtin ConfigManager"
                    << std::endl;
}

} // namespace cali::internal

} // namespace cali
