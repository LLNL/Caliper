// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "machine.h"

#include "caliper/common/Log.h"

#ifdef CALIPER_HAVE_MPI
#include "../mpi/mpi_machine.h"
#endif

namespace cali
{

namespace machine
{

int get_rank_for(MachineLevel level)
{
    switch (level) {
    case MachineLevel::Process:
        return 0;
    case MachineLevel::Node:
#ifdef CALIPER_HAVE_MPI
        return mpi::get_rank_for_node();
#else
        return 0;
#endif
    default:
        Log(0).stream() << "machine::get_rank_for(MachineLevel): level "
                        << level << " is not supported"
                        << std::endl;
    }

    return -1;
}

}

}
