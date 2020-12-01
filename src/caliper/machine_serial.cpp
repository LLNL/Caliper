// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "machine.h"

#include "caliper/common/Log.h"

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
        return 0;
    default:
        Log(0).stream() << "machine::get_rank_for(MachineLevel): level "
                        << level << " is not supported"
                        << std::endl;
    }

    return -1;
}

}

}
