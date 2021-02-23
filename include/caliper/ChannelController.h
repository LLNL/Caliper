// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

/// \file ChannelController.h
/// ChannelController class

#pragma once

#include "cali_definitions.h"

#include <map>
#include <memory>
#include <string>

namespace cali
{

class Caliper;
class Channel;

typedef std::map<std::string, std::string> config_map_t;

/// \class ChannelController
/// \ingroup ControlChannelAPI
/// \brief Base class for %Caliper channel controllers
///
/// A channel controller wraps a %Caliper configuration and channel.
/// The underlying channel is initially inactive, and will be created
/// in the first call of the start() method. Derived classes can
/// modify the configuration before the channel is created.
///
/// ChannelController objects can be copied and moved. The underlying
/// %Caliper channel will be deleted when the last ChannelController
/// object referencing is destroyed. 

class ChannelController
{
    struct ChannelControllerImpl;
    std::shared_ptr<ChannelControllerImpl> mP;

protected:

    /// \brief Return the underlying %Caliper channel object.
    Channel*      channel();

    /// \brief Provide access to the underlying config map.
    ///
    ///   Note that configuration modifications are only effective before
    /// the underlying %Caliper channel has been created.
    config_map_t& config();

    /// \brief Create the channel with the controller's
    ///   name, flags, and config map
    Channel*      create();

    /// \brief Callback function, invoked after the underlying channel
    ///   has been created.
    ///
    ///   This can be used to setup additional functionality, e.g.
    /// registering %Caliper callbacks.
    virtual void  on_create(Caliper* /*c*/, Channel* /*chn*/) { }

public:

    /// \brief Create and activate the %Caliper channel,
    ///   or reactivate a stopped %Caliper channel.
    void start();

    /// \brief Deactivate the %Caliper channel.
    void stop();

    /// \brief Returns true if channel exists and is active, false otherwise.
    bool is_active() const;

    /// \brief Returns the name of the underlying channel
    std::string name() const;

    /// \brief Flush the underlying %Caliper channel.
    ///
    ///   Allows derived classes to implement custom %Caliper data processing.
    /// The base class implementation invokes Caliper::flush_and_write().
    virtual void flush();

    /// \brief Return the underlying config map.
    config_map_t copy_config() const;

    /// \brief Create channel controller with given name, flags, and config.
    ChannelController(const char* name, int flags, const config_map_t& cfg);

    virtual ~ChannelController();
};

} // namespace cali;
