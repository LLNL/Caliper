Manual configuration
================================

When not using built-in performance measurement configurations (:doc:`BuiltinConfigurations`),
through the ConfigManager API or the `CALI_CONFIG` environment variable,
Caliper can be configured manually by setting configuration
variables through the environment or configuration files.

Configuration variables have the form

   CALI_<MODULE>_<KEY>=<VALUE>

Values can be strings, lists, numbers, or boolean values, depending on
the variable. List elements are typically separated by commas.
Boolean values can be given as '1', 'true', or '0', 'false'
for *true* and *false*, respectively.

Generally, configuring Caliper requires selecting a set of *services*,
which implement Caliper's measurement functionality.
Refer to :doc:`workflow` to learn how services can be combined to
form a complete measurement, data collection, and output pipeline.
Select services with CALI_SERVICES_ENABLE configuration variable,
for example by setting it as an environment variable:

.. code-block:: sh

   $ export CALI_SERVICES_ENABLE=event,trace,recorder,timestamp

Many services have additional configuration variables. See :doc:`services`
for a complete list of available services and their configuration.

Configuration files
--------------------------------

Configuration variables can be put in configuration files. By default,
Caliper reads the file `caliper.config` in the current working
directory, if present. Alternatively, you can provide a list of
configuration files with the CALI_CONFIG_FILE variable. Note that
CALI_CONFIG_FILE can only be set through an environment variable.

The configuration files use a simple `<VARIABLE>=<VALUE>`
syntax. Lines starting with `#` are interpreted as comments.
Example::

  # My configuration
  CALI_SERVICES_ENABLE=event,recorder,timestamp:mpi
  CALI_MPI_WHITELIST=MPI_Allreduce,MPI_Barrier
  CALI_TIMER_TIMESTAMP=true

When multiple definitions of the same variable are given, the last
definition is taken. When multiple configuration files are provided,
they are processed in the order they are given. Values set through
environment variables override values given in configuration files.

Configuration profiles
--------------------------------

Configuration profiles allow you to put multiple Caliper
configurations into a file and select one of them with a single
switch. You can declare a profile in a configuration file with ``#
[profile name]``. The following lines up to the next profile
declaration then belong to this profile. Entries that do not belong to
any specific profile are inserted into the default profile. Caliper
will use the profile given by the `CALI_CONFIG_PROFILE` variable, or
the default profile if none is set.

Example::

  CALI_CONFIG_PROFILE=callstack-trace

  # [callstack-trace]
  # A trace with call-path info
  CALI_SERVICES_ENABLE=callpath,event,recorder,timestamp
  CALI_CALLPATH_USE_NAME=true

  # [hwc-trace]
  # A trace with PAPI hardware counters
  CALI_SERVICES_ENABLE=event,papi,recorder,timestamp
  CALI_PAPI_COUNTERS=PAPI_FP_OPS

This file defines two configuration profiles, *callstack-trace* and
*hwc-trace*, and selects the callstack-trace profile. You can override
the profile selection by setting a different value in the
`CALI_CONFIG_PROFILE` environment variable. Note that environment
variables always override values set in a configuration file,
regardless of the selected profile.

Configuration variables reference
----------------------------------------

This section describes the configuration variables of the Caliper
runtime system.

Many Caliper services define additional configuration variables. See
:doc:`services` for a list of Caliper services and their
configuration.

Environment variables
........................................

These variables can only be set as environment variables.

.. envvar:: CALI_CONFIG

   A configuration string to enable one (or more) of Caliper's built-in
   performance profiling configurations. See :doc:`BuiltinConfigurations`.

.. envvar:: CALI_USE_OMPT

   Set to "1" or "true" to activate the OpenMP tools interface in the OpenMP
   runtime. This is required for the ompt service. It is only necessary to set
   this if the OpenMP runtime is first initialized before the ompt service is
   initialized, otherwise the `ompt` service will activate the OpenMP tools 
   interface automatically. When set to "0" or "false" Caliper will not use 
   the OpenMP tools interface. See :ref:`ompt <ompt-service>`

Configuration variables
........................................

These variables can be set using environment variables, config files,
or the configuration API.

CALI_CONFIG_FILE
   Comma-separated list of configuration files. The provided
   configuration files are read in order. Note: this variable can only
   be set as an environment variable or through the configuration API.

   Default: ``caliper.config``

CALI_SERVICES_ENABLE
   Comma-separated list of Caliper service modules to enable.

   Default: Not set. Caliper will not record performance data.

CALI_LOG_VERBOSITY
   | Verbosity level. Default: 1
   |   0: No output except for severe errors.
   |   1: Basic informational runtime output and warning messages.
   |   2: Debug output. Shows e.g. memory usage of context trees,
   |   trace buffers, and aggregation database.
   |   3: More debug output. Shows configuration settings.

CALI_LOG_LOGFILE
   Log file name, or 'stdout'/'stderr' for streaming to standard out or
   standard error, respectively. Default: stderr

CALI_CHANNEL_CONFIG_CHECK
   Perform basic configuration sanity checks. Caliper prints warnings
   for incomplete configurations, e.g., if a snapshot trigger service
   is enabled but no output service.

   Default: enabled (``true``)
