// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file ConfigManager.h
/// %Caliper ConfigManager class definition

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ChannelController.h"

#include "common/StringConverter.h"

namespace cali
{

/// \class ConfigManager
/// \ingroup ControlChannelAPI
/// \brief Configure, enable, and manage built-in %Caliper configurations
///
///   ConfigManager is the principal component for managing and built-in
/// %Caliper measurement configurations. It parses a configuration
/// string and creates a set of control channels for the requested
/// measurement configurations. The control channel objects can then be
/// used to start, stop, and flush the measurements channels.
/// Example:
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
/// // Activate all requested configuration channels
/// mgr.start();
///
/// // ...
///
/// //   Trigger output on all configured channel controllers.
/// // Must be done explicitly, the built-in Caliper configurations do not
/// // not flush results automatically.
/// mgr.flush();
/// \endcode
///
/// \example cxx-example.cpp
/// This example demonstrates the C++ annotation macros as well as the
/// control channel API.

class ConfigManager
{
    struct ConfigManagerImpl;
    std::shared_ptr<ConfigManagerImpl> mP;

    class OptionSpec;

public:

    typedef std::map<std::string, std::string> argmap_t;

    /// \brief Manages the list of options given to a ConfigManager config controller.
    ///   Internal use.
    class Options {
        struct OptionsImpl;
        std::shared_ptr<OptionsImpl> mP;

        Options(const OptionSpec& specs, const argmap_t& args);

    public:

        ~Options();

        /// \brief Indicates if \a option is in the options list.
        bool is_set(const char* option) const;

        /// \brief Indicates if \a option is enabled
        ///
        /// An option is enabled if it is present in the options list, and,
        /// for boolean options, set to \a true.
        bool is_enabled(const char* option) const;

        /// \brief Return the value for \a option, or \a default_value
        ///   if it is not set in the options list.
        StringConverter get(const char* option, const char* default_value = "") const;

        /// \brief Perform a validity check.
        std::string check() const;

        /// \brief Return a list of all enabled boolean options.
        std::vector<std::string> enabled_options() const;

        /// \brief Update the config controller's %Caliper configuration
        ///   according to the requirements of the selected options.
        ///
        /// Updates CALI_SERVICES_ENABLE and adds any additional
        /// configuration flags that may be required.
        void update_channel_config(config_map_t& config) const;

        /// \brief Returns a CalQL SELECT expression to compute metrics
        ///   in the option list.
        /// \param level The aggregation level ('serial', 'local', or 'cross')
        /// \param in Base SELECT expression as required by the controller
        /// \param use_alias Wether to add aliases to the expression
        ///   (as in SELECT x AS y)
        /// \return The SELECT list (comma-separated string), but without the
        ///   'SELECT' keyword
        std::string
        query_select(const char* level, const std::string& in, bool use_alias = true) const;

        /// \brief Returns a CalQL GROUP BY list for metrics in the option
        ///   list.
        /// \param level The aggregation level ('serial', 'local', or 'cross')
        /// \param in Base GROUP BY list as required by the controller
        /// \return The GROUP BY list (comma-separated string), but without the
        ///   'GROUP BY' keyword
        std::string
        query_groupby(const char* level, const std::string& in) const;

        friend class ConfigManager;
    };

    typedef cali::ChannelController* (*CreateConfigFn)(const Options&);
    typedef std::string              (*CheckArgsFn)(const Options&);

    struct ConfigInfo {
        const char*    spec;
        CreateConfigFn create;
        CheckArgsFn    check_args;
    };

    /// \brief Add a list of pre-defined configurations. Internal use.
    static void
    add_controllers(const ConfigInfo**);
    /// \brief Clean up controller list. Internal use.
    static void
    cleanup();

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
    ///
    /// In this add() version, key-value pairs in the config string that
    /// neither represent a valid configuration or configuration parameter
    /// will be marked as a parse error.
    ///
    /// \return false if there was a parse error, true otherwise
    bool add(const char* config_string);

    /// \brief Parse the \a config_string configuration string and add the
    ///   specified configuration channels.
    ///
    /// Works similar to ConfigManager::add(const char*), but does not mark
    /// extra key-value pairs in the config string that do not represent a
    /// configuration name or parameter as errors, and instead returns them in
    /// \a extra_kv_pairs.
    bool add(const char* config_string, argmap_t& extra_kv_pairs);

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

    /// \brief Start all configured measurement channels, or re-start
    ///   paused ones
    ///
    /// Invokes the ChannelController::start() method on all configuration
    /// channel controllers created by the ConfigManager. Equivalent to
    /// \code
    /// ConfigManager mgr;
    /// // ...
    /// auto channels = mgr.get_all_channels();
    /// for (auto& channel : channels)
    ///     channel->start();
    /// \endcode
    void
    start();

    /// \brief Pause all configured measurement channels
    ///
    /// Invokes the ChannelController::stop() method on all configuration
    /// channel controllers created by the ConfigManager.
    void
    stop();

    /// \brief Flush all configured measurement channels
    ///
    /// Invokes the ChannelController::flush() method on all configuration
    /// channel controllers created by the ConfigManager. Equivalent to
    /// \code
    /// auto channels = mgr.get_all_channels();
    /// for (auto& channel : channels)
    ///     channel->flush();
    /// \endcode
    void
    flush();

    /// \brief Return names of available configs
    static std::vector<std::string>
    available_configs();

    /// \brief Return descriptions for all available configs
    static std::vector<std::string>
    get_config_docstrings();

    /// \brief Check if given config string is valid.
    ///
    /// If \a allow_extra_kv_pairs is set to \t false, extra key-value pairs
    /// in the config string that do not represent configurations or parameters
    /// will be marked as errors.
    ///
    /// \return error message, or empty string if input is valid.
    static std::string
    check_config_string(const char* config_string, bool allow_extra_kv_pairs = false);
};

} // namespace cali
