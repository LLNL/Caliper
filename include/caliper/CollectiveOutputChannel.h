// Copyright (c) 2021, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

/// \file CollectiveOutputChannel.h
/// CollectiveOutputChannel class

#pragma once

#ifndef CALI_COLLECTIVE_CHANNEL_CONTROLLER_H
#define CALI_COLLECTIVE_CHANNEL_CONTROLLER_H

#include "ChannelController.h"

#include <mpi.h>

#include <memory>

namespace cali
{

/// \brief A ChannelController that aggregates output over MPI
class CollectiveOutputChannel : public cali::ChannelController
{
    struct CollectiveOutputChannelImpl;
    std::shared_ptr<CollectiveOutputChannelImpl> mP;

public:

    /// \brief Create channel controller with given name, flags, and config.
    CollectiveOutputChannel(const char* name, int flags, const config_map_t& cfg);

    static CollectiveOutputChannel from(const cali::ChannelController& from);

    virtual std::ostream& collective_flush(std::ostream& os, MPI_Comm comm);

    virtual void flush() override;

    virtual ~CollectiveOutputChannel();
};

} // namespace cali

#endif
