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
#ifdef CALIPER_HAVE_MPIT
extern CaliperService mpit_service;
#endif
#ifdef CALIPER_HAVE_TAU
extern CaliperService tau_service;
#endif

CaliperService cali_mpi_services[] = {
    mpiwrap_service,
    mpireport_service,
#ifdef CALIPER_HAVE_MPIT
    mpit_service,
#endif
#ifdef CALIPER_HAVE_TAU
    tau_service,
#endif
    { nullptr, nullptr }
};

extern ConfigManager::ConfigInfo spot_controller_info;
extern ConfigManager::ConfigInfo spot_v1_controller_info;

const ConfigManager::ConfigInfo *mpi_controllers[] = {
    &spot_controller_info,
    &spot_v1_controller_info,
    nullptr
};

bool is_initialized = false;

//   Pre-init setup routine that allows us to do some MPI-specific
// initialization, e.g. disabling most logging on non-rank 0 ranks.
//
//   This is called by Caliper::init() _before_ Caliper is initialized:
// can't use the Caliper API here!

void
setup_mpi()
{
    int is_initialized = 0;
    MPI_Initialized(&is_initialized);

    if (is_initialized) {
        int rank = 0;

        MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        // Disable (most) logging on non-0 ranks by default
        std::ostringstream logprefix;
        logprefix << "(" << rank << "): ";

        Log::add_prefix(logprefix.str());

        if (rank > 0)
            Log::set_verbosity(0);
    }
}


void mpirt_constructor() __attribute__((constructor));

void
mpirt_constructor()
{
    if (is_initialized)
        return;

    Caliper::add_services(cali_mpi_services);
    Caliper::add_init_hook(setup_mpi);

    ConfigManager::add_controllers(mpi_controllers);

    is_initialized = true;
}

}

extern "C"
{

void
cali_mpi_init()
{
    ::mpirt_constructor();
}

}
