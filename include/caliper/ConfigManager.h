// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

/// \file ConfigManager.h
/// %Caliper ConfigManager class definition

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cali
{

class ChannelController;

/// \brief Configure, enable, and manage built-in %Caliper configurations
///
///   ConfigManager creates channel control objects for built-in %Caliper
/// configurations based on given configuration strings. Example:
///
/// \code
/// cali::ConfigManager mgr;
///
/// //   Add a configuration string creating a runtime report
/// // and event trace channel
/// mgr.add("runtime-report,event-trace(output=trace.cali)");
///
/// // Check for configuration string parse errors
/// if (mgr.error()) {
///     std::cerr << "ConfigManager: " << mgr.error_msg() << std::endl;
/// }
///
/// //   Get channel controller objects for the requested configurations
/// // and activate them
/// auto channels = mgr.get_all_channels();
/// for (auto &c : channels)
///     c->start();
///
/// // ...
///
/// //   Trigger output on the configured channel controllers.
/// // Must be done explicitly, the built-in Caliper configurations do not
/// // not flush results automatically.
/// for (auto &c : channels)
///     c->flush();
/// \endcode
class ConfigManager
{
    struct ConfigManagerImpl;
    std::shared_ptr<ConfigManagerImpl> mP;
    
public:
    
    typedef std::map<std::string, std::string> argmap_t;

    typedef cali::ChannelController* (*CreateConfigFn)(const argmap_t&);

    struct ConfigInfo {
        const char*    name;
        const char**   args;
        CreateConfigFn create;
    };

    /// \brief Add a list of pre-defined configurations. Internal use. 
    static void
    add_controllers(const ConfigInfo*);

    ConfigManager();

    /// \brief Construct ConfigManager and add the given configuration string.
    explicit ConfigManager(const char* config_string);

    ~ConfigManager();

    /// \brief Parse the \a config_string configuration string and add the
    ///   specified configuration channels.
    ///
    /// Parses configuration strings of the following form:
    /// 
    ///   <config> ( <argument> = value, ... ), ...
    ///
    /// e.g., "runtime-report,event-trace(output=trace.cali)"
    ///
    /// If there was an error parsing the configuration string, the error()
    /// method will return \a true and an error message can be retrieved
    /// with error_msg().
    ///
    /// If the configuration string was parsed successfully, ChannelController
    /// instances for the requested configurations will be created and can be
    /// accessed through get_all_channels() or get_channel(). The channels are
    /// initially inactive and must be activated explicitly with
    /// ChannelController::start().
    ///
    /// add() can be invoked multiple times.
    bool add(const char* config_string);

    /// \brief Pre-set parameter \a key to \a value for all configurations
    void set_default_parameter(const char* key, const char* value);

    /// \brief Returns \a true if there was an error parsing configuration
    ///   strings 
    bool error() const;

    /// \brief Returns an error message if there was an error parsing
    ///   configuration strings
    std::string error_msg() const;
    
    typedef std::shared_ptr<cali::ChannelController> ChannelPtr;
    typedef std::vector<ChannelPtr> ChannelList;

    /// \brief Return a list of channel controller instances for the requested
    ///   configurations
    ///
    /// \return An STL container with C++ shared_ptr objects to the
    /// ChannelController instances created from the configuration strings.
    ChannelList
    get_all_channels();

    /// \brief Return a channel controller instance for configuration \a name
    ///
    /// Returns a C++ shared pointer containing the channel controller instance
    /// with the given \a name, or an empty shared_ptr object when no such
    /// channel exists.
    ChannelPtr
    get_channel(const char* name);
};

} // namespace cali
