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
/// \brief Base class for Caliper controllers
class ChannelController
{
    struct ChannelControllerImpl;
    std::shared_ptr<ChannelControllerImpl> mP;

protected:

    /// \brief Return the underlying %Caliper channel object.
    Channel*      channel();

    /// \brief Provide access to the underlying config map.
    config_map_t& config();

    /// \brief Create the channel with the controller's
    ///   name, flags, and config map
    Channel*      create();

    /// \brief Callback function, invoked after the underlying channel
    ///   has been created.
    virtual void  on_create(Caliper* c, Channel* chn) { }

public:

    /// \brief Create and activate the %Caliper channel,
    ///   or reactivate a stopped %Caliper channel.
    void start();

    /// \brief Deactivate the %Caliper channel.
    void stop();

    /// \brief Returns true if channel exists and is active, false otherwise.
    bool is_active() const;

    /// \brief Flush the underlying %Caliper channel
    virtual void flush();

    /// \brief Create channel controller with given name, flags, and config.
    ChannelController(const char* name, int flags, const config_map_t& cfg);

    virtual ~ChannelController();
};

} // namespace cali;
