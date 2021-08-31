// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file MpiChannelManager.h
/// MpiChannelManagerWrapper class

#pragma once

#ifndef CALI_MPI_CHANNEL_MANAGER_H
#define CALI_MPI_CHANNEL_MANAGER_H

#include <mpi.h>

#include <memory>

namespace cali
{

class ChannelController;
class ConfigManager;

/// \brief Manage ConfigManager channels that run on a user-defined MPI
///   communicator
///
/// An MpiChannelManager imports Caliper control channel configurations
/// from a ConfigManager or other ChannelController objects, but runs their
/// MPI operations (specifically, flush) on a user-provided MPI
/// communicator.
///
/// Example:
///
/// \code
/// int worldrank;
/// MPI_Comm subcomm;
/// MPI_Comm_rank(MPI_COMM_WORLD, &worldrank);
/// MPI_Comm_split(MPI_COMM_WORLD, worldrank % 2, worldrank, &subcomm);
///
/// cali::ConfigManager mgr("runtime-report,profile.mpi");
/// cali::MpiChannelManager mpimgr(subcomm);
/// mpimgr.add(mgr);
///
/// if (worldrank % 2 == 0)
///   mpimgr.start();
///
/// MPI_Barrier(subcomm);
///
/// if (worldrank % 2 == 0)
///   mpimgr.collective_flush(); // flush() runs on subcomm
/// \endcode

class MpiChannelManager
{
    struct MpiChannelManagerImpl;
    std::shared_ptr<MpiChannelManagerImpl> mP;

public:

    /// \brief Create channel manager on communicator \a comm
    explicit MpiChannelManager(MPI_Comm comm);

    ~MpiChannelManager();

    /// \brief Import all channels from \a mgr
    /// \sa MpiChannelManager::add(const std::shared_ptr<ChannelController>& src)
    void add(const ConfigManager& mgr);

    /// \brief Import channel configuration from \a src
    ///
    /// This creates a new channel with the same configuration as \a src
    /// that will run MPI operations (if any) on the given MPI communicator.
    void add(const std::shared_ptr<ChannelController>& src);

    /// \brief Start all channels
    /// \sa ChannelController::start()
    void start();

    /// \brief Stop all channels
    /// \sa ChannelController::stop()
    void stop();

    /// \brief Flush all channels
    ///
    /// This is a collective operation on this channel's MPI communicator.
    void collective_flush();
};

} // namespace cali

#endif
