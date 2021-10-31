// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Caliper runtime MPI setup function: Sets log verbosity etc.

#include "caliper/caliper-config.h"

#include "OutputCommMpi.h"

#include "caliper/Caliper.h"
#include "caliper/CaliperService.h"
#include "caliper/CustomOutputController.h"

#include "caliper/common/Log.h"
#include "caliper/common/OutputStream.h"

#include <mpi.h>

#include <sstream>

using namespace cali;

using COC = cali::internal::CustomOutputController;

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

// Implement flush over MPI for CustomOutputController objects
void
custom_output_controller_flush_mpi(COC* controller)
{
    Log(2).stream() << controller->name() << ": CustomOutputController::flush(): using MPI" << std::endl;

    OutputCommMpi comm;
    OutputStream  stream;

    controller->collective_flush(stream, comm);
}

} // namespace [anonymous]

namespace cali
{
namespace mpi
{

void
add_mpi_controllers_and_services()
{
    const CaliperService services[] = {
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

    Caliper::add_services(services);

    COC::set_flush_fn(::custom_output_controller_flush_mpi);
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
