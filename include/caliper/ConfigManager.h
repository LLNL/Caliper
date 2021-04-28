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
/// \brief Configure, enable, and manage built-in or custom %Caliper configurations
///
///   ConfigManager is the principal component for managing %Caliper
/// measurement configurations programmatically. It parses a configuration
/// string and creates a set of control channels for the requested
/// measurement configurations, and provides control methods
/// to start, stop, and flush the created measurements channels.
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
///   ConfigManager provides a set of built-in configurations specifications
/// like "runtime-report". Users can also add custom specifications with
/// add_config_spec().
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

        /// \brief Returns a CalQL query based on the fields in \a input
        ///   and option list.
        ///
        /// Construct a CalQL query for the option list. Example:
        /// \code
        /// auto query = opts.build_query("cross", {
        ///         { "select",   "sum(inclusive#sum#time.duration) as Total unit sec" },
        ///         { "group by", "prop:nested" },
        ///         { "format",   "tree" }
        ///     });
        /// \endcode
        ///
        /// \param level The aggreatation level ("local" or "cross")
        /// \param in Base CalQL clauses as needed by the controller
        /// \return Complete CalQL query statement
        std::string
        build_query(const char* level, const std::map<std::string, std::string>& in, bool use_alias = true) const;

        friend class ConfigManager;
    };

    /// \brief Callback function to create a custom ChannelController for a config.
    ///
    /// Example:
    /// \code
    /// cali::ChannelController* make_controller(const char* name,
    ///         const cali::config_map_t& initial_cfg,
    ///         const cali::ConfigManager::Options& opts)
    /// {
    ///     class MyChannelController : public cali::ChannelController {
    ///     public:
    ///         MyChannelController(const char* name,
    ///                 const cali::config_map_t& initial_cfg,
    ///                 const cali::ConfigManager::Options& opts)
    ///             : cali::ChannelController(name, 0, initial_cfg)
    ///         {
    ///             opts.update_channel_config(config());
    ///         }
    ///     };
    ///
    ///     return new MyChannelController(name, initial_cfg, opts);
    /// }
    /// \endcode
    ///
    /// \param name The channel name. Must be passed to the created
    ///    ChannelController object.
    /// \param initial_cfg The initial config map from the config's JSON spec.
    ///    Must be passed to the created ChannelController object.
    /// \param opts User-requested options for the channel. Invoke
    ///    Options::update_channel_config(config_map_t&) const
    ///    on the ChannelController's config map to apply options.
    /// \return A new ChannelController object.
    typedef cali::ChannelController* (*CreateConfigFn)(const char* name, const config_map_t& initial_cfg, const Options& opts);
    /// \brief Callback function to implement custom options checking for a config spec.
    typedef std::string              (*CheckArgsFn)(const Options& opts);

    /// \brief Define a config spec with custom ChannelController creation
    ///   and option checking functions
    struct ConfigInfo {
        /// \brief JSON config spec. \see ConfigManager::add_config_spec(const char*)
        const char*    spec;
        /// \brief Optional custom ChannelController creation function,
        ///   or \a nullptr to use the default.
        CreateConfigFn create;
        /// \brief Optional argument checking function, or \a nullptr
        ///   to use the default.
        CheckArgsFn    check_args;
    };

    ConfigManager();

    /// \brief Construct ConfigManager and add the given configuration string.
    explicit ConfigManager(const char* config_string);

    ~ConfigManager();

    /// \brief Add a custom config spec to this ConfigManager
    ///
    /// Adds a new %Caliper configuration specification for this ConfigManager
    /// using a custom ChannelController or option checking function.
    void add_config_spec(const ConfigInfo& info);

    /// \brief Add a JSON config spec to this ConfigManager
    ///
    /// Adds a new %Caliper configuration specification for this ConfigManager
    /// using a basic ChannelController.
    ///
    /// This example adds a config spec to perform simple sample tracing:
    /// \code
    /// const char* spec =
    ///   "{"
    ///   " \"name\"        : \"sampletracing\","
    ///   " \"description\" : \"Perform sample tracing\","
    ///   " \"services\"    : [ \"recorder\", \"sampler\", \"trace\" ],"
    ///   " \"config\"      :
    ///   "  { \"CALI_SAMPLER_FREQUENCY\"     : \"100\",  "
    ///   "    \"CALI_CHANNEL_FLUSH_ON_EXIT\" : \"false\" "
    ///   "  },"
    ///   " \"categories\"  : [ \"output\" ],"
    ///   " \"defaults\"    : { \"sample.threads\": \"true\" }, "
    ///   " \"options\"     : "
    ///   " [ "
    ///   "  { \"name\"        : \"sample.threads\","
    ///   "    \"description\" : \"Sample all threads\","
    ///   "    \"type\"        : \"bool\","
    ///   "    \"services\"    : [ \"pthread\" ]"
    ///   "  }"
    ///   " ]"
    ///   "}";
    ///
    /// ConfigManager mgr;
    /// mgr.add_config_spec(spec);
    ///
    /// // Add a thread sampling channel using the spec and start recording
    /// mgr.add("sampletracing(sample.threads=true,output=trace.cali)");
    /// mgr.start();
    /// // ...
    /// mgr.flush();
    /// \endcode
    ///
    /// \par Config specification syntax
    /// The config spec is a JSON dictionary with the following elements:
    /// \li \a name: Name of the config spec.
    /// \li \a description: A short one-line description.
    ///   Included in the documentation string generated by
    ///   get_documentation_for_spec().
    /// \li \a services: List of %Caliper services this config requires.
    ///   Note that the config will only be available if all required
    ///   services are present in %Caliper.
    /// \li \a config: A dictionary with %Caliper configuration
    ///   variables required for this config. Note that services will be
    ///   added automatically based on the \a services entry.
    /// \li \a categories: A list of option categories. Defines which
    ///   options, in addition to the ones defined inside the config spec,
    ///   apply to this config. The example above
    ///   uses the "output" category, which makes the built-in \a output
    ///   option for setting output file names available.
    /// \li \a defaults: A dict with default values for any of the
    ///   config's options. Options not listed here default to empty/not set.
    /// \li \a options: A list of custom options for this config.
    ///
    /// \see add_option_spec()
    ///
    /// If there was an error parsing the config spec, the error() method will
    /// return \a true and an error message can be retrieved with error_msg().
    void add_config_spec(const char* json);

    /// \brief Add a JSON option spec to this %ConfigManager
    ///
    /// Allows one to define options for any config in a matching category.
    /// Option specifications must be added before querying or creating any
    /// configurations to be effective.
    /// If there was an error parsing the config spec, the error() method will
    /// return \a true and an error message can be retrieved with error_msg().
    /// The following example adds a metric
    /// option to compute instructions counts using the papi service:
    ///
    /// \code
    /// const char* spec =
    ///   "{"
    ///   " \"name\"        : \"count.instructions\","
    ///   " \"category\"    : \"metric\","
    ///   " \"description\" : \"Count total instructions\","
    ///   " \"services\"    : [ \"papi\" ],"
    ///   " \"config\"      : { \"CALI_PAPI_COUNTERS\": \"PAPI_TOT_INS\" },"
    ///   " \"query\"       : "
    ///   " ["
    ///   "  { \"level\": \"local\", \"select\":"
    ///   "   [ { \"expr\": \"sum(sum#papi.PAPI_TOT_INS)\" } ]"
    ///   "  },"
    ///   "  { \"level\": \"cross\", \"select\":"
    ///   "   [ { \"expr\": \"avg(sum#sum#papi.PAPI_TOT_INS)\", \"as\": \"Avg instr./rank\" },"
    ///   "     { \"expr\": \"max(sum#sum#papi.PAPI_TOT_INS)\", \"as\": \"Max instr./rank\" }"
    ///   "   ]"
    ///   "  }"
    ///   " ]"
    ///   "}";
    ///
    /// ConfigManager mgr;
    /// mgr.add_option_spec(spec);
    ///
    /// //   Create a runtime-report channel using the count.instructions option
    /// // and start recording.
    /// mgr.add("runtime-report(count.instructions)");
    /// mgr.start();
    /// \endcode
    ///
    /// \par Option specification syntax
    /// The option spec is a JSON dictionary with the following elements:
    /// \li \a name: Name of the option.
    /// \li \a category: The option's category. The category defines
    ///   which configs can use this option: An option is only available
    ///   to configs which list this category in their "categories"
    ///   setting.
    /// \li \a description: A short one-line description.
    ///   Included in the documentation string generated by
    ///   get_documentation_for_spec().
    /// \li \a services: List of %Caliper services this option requires.
    ///   Note that the option will only be available if all required
    ///   services are present in %Caliper.
    /// \li \a config: A dictionary with %Caliper configuration
    ///   variables required for this option. Note that services will be
    ///   added automatically based on the \a services entry.
    /// \li \a query: Defines aggregation operations to compute
    ///   performance metrics. Specific to "metric" options. There are
    ///   two aggregation levels: \e local computes process-local
    ///   metrics, and \e cross computes cross-process
    ///   metrics in MPI programs. For each level, specify metrics
    ///   using a list of "select" definitions, where \e expr defines an
    ///   aggregation using a CalQL expression, and \e as provides a
    ///   human-readable name for the metric. Metrics on the \e serial
    ///   and \e local levels use runtime aggregation results from
    ///   the "aggregate" service as input, metrics on the \e cross level
    ///   use "local" metrics as input.
    void add_option_spec(const char* json);

    /// \brief Parse the \a config_string configuration string and create the
    ///   specified configuration channels.
    ///
    /// Parses configuration strings of the following form:
    ///
    ///   <config> ( <option> = value, ... ), ...
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
    /// ConfigManager::start().
    ///
    /// add() can be invoked multiple times.
    ///
    /// In this add() version, key-value pairs in the config string that
    /// neither represent a valid configuration or configuration option
    /// will be marked as a parse error.
    ///
    /// \return false if there was a parse error, true otherwise
    bool add(const char* config_string);

    /// \brief Parse the \a config_string configuration string and create the
    ///   specified configuration channels.
    ///
    /// Works similar to ConfigManager::add(const char*), but does not mark
    /// extra key-value pairs in the config string that do not represent a
    /// configuration name or option as errors, and instead returns them in
    /// \a extra_kv_pairs.
    bool add(const char* config_string, argmap_t& extra_kv_pairs);

    /// \brief Load config and option specs from \a filename
    ///
    /// Loads JSON config and option specs from a file. The files can contain
    /// either a single config spec, a list of config specs, or a JSON object
    /// with both config and option specs in the following form:
    ///
    /// \code
    /// {
    ///  "configs": [
    ///   { "name": "myconfig", ... }, ...
    ///  ],
    ///  "options": [
    ///   { "name": "myoption", "category": ... }, ...
    ///  ]
    /// }
    /// \endcode
    ///
    /// The schemas for config and option specs are described in
    /// ConfigManager::add_config_spec(const char*) and
    /// ConfigManager::add_option_spec(const char*), respectively.
    ///
    /// If there was an error parsing the file, the error()
    /// method will return \a true and an error message can be retrieved
    /// with error_msg().
    ///
    /// \sa ConfigManager::add_config_spec(const char*),
    ///   ConfigManager::add_option_spec(const char*)
    void load(const char* filename);

    /// \brief Pre-set parameter \a key to \a value for all configurations
    void set_default_parameter(const char* key, const char* value);

    /// \brief Pre-set parameter \a key to \a value for \a config
    void set_default_parameter_for_config(const char* config, const char* key, const char* value);

    /// \brief Returns \a true if there was an error parsing configuration
    ///   strings
    bool error() const;

    /// \brief Returns an error message if there was an error parsing
    ///   configuration strings
    std::string error_msg() const;

    typedef std::shared_ptr<cali::ChannelController> ChannelPtr;
    typedef std::vector<ChannelPtr> ChannelList;

    /// \brief Parse \a config_string and return the specified configuration
    ///   channels
    ///
    /// Unlike ConfigManager::add(const char*), this function does not add
    /// the specified configuration channels to the ConfigManager object's
    /// internal list of channels.
    ///
    /// If there was an error parsing the configuration string, the error()
    /// method will return \a true and an error message can be retrieved
    /// with error_msg().
    ///
    /// \return List of configuration channels
    ChannelList parse(const char* config_string);

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

    /// \brief Check if the given config string is valid.
    ///
    /// If \a allow_extra_kv_pairs is set to \t false, extra key-value pairs
    /// in the config string that do not represent configurations or parameters
    /// will be marked as errors.
    ///
    /// \return Error message, or empty string if input is valid.
    std::string
    check(const char* config_string, bool allow_extra_kv_pairs = false) const;

    /// \brief Return names of available config specs
    ///
    /// Returns only the specifications whose requirements (e.g., available services)
    /// are met in this %Caliper instance.
    ///
    /// \return Names of all available config specs for this ConfigManager.
    std::vector<std::string>
    available_config_specs() const;

    /// \brief Return description and options for the given config spec.
    std::string
    get_documentation_for_spec(const char* name) const;

    /// \brief Return names of global config specs.
    ///
    /// \deprecated Query specific ConfigManager object instead.
    static std::vector<std::string>
    available_configs();

    /// \brief Return descriptions for available global configs.
    static std::vector<std::string>
    get_config_docstrings();

    /// \brief Check if given config string is valid for global config specs.
    ///   Deprecated.
    ///
    /// If \a allow_extra_kv_pairs is set to \t false, extra key-value pairs
    /// in the config string that do not represent configurations or parameters
    /// will be marked as errors.
    ///
    /// \deprecated Create a ConfigManager object and use its check() method.
    ///
    /// \return Error message, or empty string if input is valid.
    static std::string
    check_config_string(const char* config_string, bool allow_extra_kv_pairs = false);
};

/// \brief Add a set of global ConfigManager configs
/// \ingroup ControlChannelAPI
void add_global_config_specs(const ConfigManager::ConfigInfo** configs);

} // namespace cali
