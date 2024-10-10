// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "OutputCommMpi.h"

#include "caliper/CaliperService.h"
#include "caliper/CustomOutputController.h"

#include "caliper/common/Log.h"
#include "caliper/common/OutputStream.h"

#include "../services/Services.h"

#include <mpi.h>

#include <sstream>

using namespace cali;

using COC = cali::internal::CustomOutputController;

namespace
{

void setup_log_prefix()
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
void custom_output_controller_flush_mpi(COC* controller)
{
    Log(2).stream() << controller->name() << ": CustomOutputController::flush(): using MPI" << std::endl;

    OutputCommMpi comm;
    OutputStream  stream;

    controller->collective_flush(stream, comm);
}

} // namespace

namespace cali
{

extern CaliperService mpiflush_service;

void add_submodule_controllers_and_services()
{
    static const CaliperService mpi_services[] = { mpiflush_service, { nullptr, nullptr } };

    services::add_service_specs(mpi_services);
    services::add_default_service_specs();
}

void init_submodules()
{
    ::setup_log_prefix();
    COC::set_flush_fn(::custom_output_controller_flush_mpi);

    add_submodule_controllers_and_services();
}

} // namespace cali

extern "C"
{

void cali_mpi_init()
{
    ::setup_log_prefix();
}
}
