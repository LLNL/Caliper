// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file  MpiEvents.h
/// \brief Caliper-MPI internal callbacks

#include "caliper/common/util/callback.hpp"

namespace cali
{

class Caliper;
class Channel;

/// \brief The MPI callbacks
struct MpiEvents {
    typedef util::callback<void(Caliper*, Channel* chn)> 
        mpi_env_cbvec;

    /// \brief MPI has been initialized. 
    /// 
    ///   This callback will be called by the MPI wrapper service (mpi) once 
    /// when MPI is initialized. This may happen during or after MPI_Init().
    mpi_env_cbvec mpi_init_evt;

    /// \brief MPI is about to be finalized.
    mpi_env_cbvec mpi_finalize_evt;
};

/// \brief Return the MpiEvents instance for the given channel.
MpiEvents& mpiwrap_get_events(Channel* chn); // defined in mpiwrap/Wrapper.w

}
