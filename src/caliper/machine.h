// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file machine.h
/// Machine utility functions (internal API)

#pragma once

#ifndef CALI_CALIPER_MACHINE_H
#define CALI_CALIPER_MACHINE_H

namespace cali
{

namespace machine
{

/// \brief Describe the hardware level
enum MachineLevel {
    None, Process, Socket, Node
};

/// \brief Determine rank of the calling process/thread on the given \a level
int get_rank_for(MachineLevel level);

} // namespace machine

} // namespace cali

#endif
