// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file mpi-machine.h
/// MPI machine utility functions (internal API)

#pragma once

#ifndef CALI_MPI_MACHINE_H
#define CALI_MPI_MACHINE_H

namespace cali
{

namespace mpi
{

/// \brief Determine rank of the calling process for the local node
int get_rank_for_node();

} // namespace mpi

} // namespace cali

#endif
