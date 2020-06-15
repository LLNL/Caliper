// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "../services/Services.h"

namespace cali
{

#ifdef CALIPER_HAVE_MPI
namespace mpi
{

extern void setup_mpi();
extern void add_mpi_controllers_and_services();

}
#endif

void add_submodule_controllers_and_services()
{
    services::add_default_service_specs();
#ifdef CALIPER_HAVE_MPI
    mpi::add_mpi_controllers_and_services();
#endif
}

void init_submodules()
{
#ifdef CALIPER_HAVE_MPI
    mpi::setup_mpi();
#endif
    add_submodule_controllers_and_services();
}

} // namespace cali
