// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Caliper runtime MPI setup function: Sets log verbosity etc.

#include "caliper/caliper-config.h"

#include "caliper/Caliper.h"
#include "caliper/CaliperService.h"
#include "caliper/ConfigManager.h"

#include "caliper/common/Log.h"

#include <mpi.h>

#include <sstream>

using namespace cali;

namespace cali
{

extern CaliperService mpiwrap_service;
extern CaliperService mpireport_service;
extern CaliperService mpiflush_service;
#ifdef CALIPER_HAVE_MPIT
extern CaliperService mpit_service;
#endif
#ifdef CALIPER_HAVE_TAU
extern CaliperService tau_service;
#endif

extern ConfigManager::ConfigInfo loop_report_controller_info;
extern ConfigManager::ConfigInfo spot_controller_info;
extern ConfigManager::ConfigInfo spot_v1_controller_info;
}

namespace
{

void
setup_log_prefix()
{
    static bool done = false;

    if (done)
        return;

    int mpi_is_initialized = 0;
    MPI_Initialized(&mpi_is_initialized);

    if (mpi_is_initialized && Log::is_initialized()) {
        int rank = 0;

        MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        // Disable (most) logging on non-0 ranks by default
        std::ostringstream logprefix;
        logprefix << "(" << rank << "): ";

        Log::add_prefix(logprefix.str());

        if (rank > 0)
            Log::set_verbosity(0);

        done = true;
    }
}

} // namespace [anonymous]

namespace cali
{
namespace mpi
{

void
add_mpi_controllers_and_services()
{
    CaliperService services[] = {
        mpiwrap_service,
        mpireport_service,
        mpiflush_service,
#ifdef CALIPER_HAVE_MPIT
        mpit_service,
#endif
#ifdef CALIPER_HAVE_TAU
        tau_service,
#endif
        { nullptr, nullptr }
    };

    const ConfigManager::ConfigInfo* controllers[] = {
        &loop_report_controller_info,
        &spot_controller_info,
        &spot_v1_controller_info,
        nullptr
    };

    Caliper::add_services(services);
    add_global_config_specs(controllers);
}

void
setup_mpi()
{
    ::setup_log_prefix();
}

} // namespace mpi
} // namespace cali


extern "C"
{

void
cali_mpi_init()
{
    ::setup_log_prefix();
}

}
