Configuration
================================

This section describes the general configuration of the Caliper
runtime system and lists the core system's configuration
variables. See next section for the list of measurement services and
their configuration.

**Note**: by default, Caliper does not activate any measurement or
output/processing services. Unless you run with third-party tool
that uses the Caliper API to retrieve data you will at least need to
select a configuration profile to obtain output.

----------------------------------------
Configuring the Caliper runtime system
----------------------------------------

The Caliper runtime system can be configured through environment
variables and/or configuration files. In addition, Caliper comes with
some built-in configuration profiles.

Configuration variables
--------------------------------

In general, Caliper configuration variables have the form

.. code-block:: sh

                CALI_<MODULE>_<KEY>=<VALUE>

Values can be strings, lists, numbers, or boolean values, depending on
the variable. List elements are typically separated by the colon (':')
character. Boolean values can be given as '1', 'true', or '0', 'false'
for *true* and *false*, respectively.

Set caliper configuration variables through environment variables,
e.g.

.. code-block:: sh

                $ export CALI_LOG_VERBOSITY=2

or configuration files (see below).   


Configuration files
--------------------------------

Configuration variables can be put in configuration files. By default,
Caliper reads the file `caliper.config` in the current working
directory, if present. Alternatively, you can provide a list of
configuration files with the `CALI_CONFIG_FILE` variable. Note that
`CALI_CONFIG_FILE` can only be set through an environment variable.

The configuration files use a simple `<VARIABLE>=<VALUE>`
syntax. Lines starting with `#` are interpreted as comments.
Example::

  # My configuration
  CALI_SERVICES_ENABLE=event:recorder:timestamp:mpi
  CALI_MPI_WHITELIST=MPI_Allreduce:MPI_Barrier
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
  CALI_SERVICES_ENABLE=callpath:event:recorder:timestamp
  CALI_CALLPATH_USE_NAME=true

  # [hwc-trace]
  # A trace with PAPI hardware counters
  CALI_SERVICES_ENABLE=event:papi:recorder:timestamp
  CALI_PAPI_COUNTERS=PAPI_FP_OPS

This file defines two configuration profiles, *callstack-trace* and
*hwc-trace*, and selects the callstack-trace profile. You can override
the profile selection by setting a different value in the
`CALI_CONFIG_PROFILE` environment variable. Note that environment
variables always override values set in a configuration file,
regardless of the selected profile.

Built-in configuration profiles
--------------------------------

Caliper provides built-in configuration profiles for common use
cases. Currently, the following profiles are defined:

============  =============
Profile name  Variables set
============  =============
serial-trace  CALI_SERVICES_ENABLE=event:recorder:timestamp:trace
thread-trace  CALI_SERVICES_ENABLE=event:pthread:recorder:timestamp:trace
mpi-trace     CALI_SERVICES_ENABLE=event:mpi:pthread:recorder:timestamp:trace
============  =============

Configuration profiles, including built-in profiles, can be
extended. For example, the following configuration file ::

  # [mpi-trace]
  CALI_MPI_WHITELIST=MPI_Allreduce

adds the ``CALI_MPI_WHITELIST`` entry to the built-in `mpi-trace`
profile.

----------------------------------------
Configuration variables reference
----------------------------------------

This section describes the configuration variables of the Caliper
runtime system.

.. envvar:: CALI_CONFIG_PROFILE = <profile name>
            
   Configuration profile name. 

.. envvar:: CALI_CONFIG_FILE = <list of config files>
            
   Colon-separated list of configuration files. The provided
   configuration files are read in order. Note: this variable can only
   be set as an environment variable.

   Default: ``caliper.config``

.. envvar:: CALI_LOG_VERBOSITY = (0|1|2)
            
   | Verbosity level. Default: 1
   |   0: no output
   |   1: basic informational runtime output
   |   2: debug output

.. envvar:: CALI_LOG_LOGFILE = (stdout|stderr|<filename>)
            
   Log file name, or 'stdout'/'stderr' for streaming to standard out or
   standard error, respectively. Default: stderr

.. envvar:: CALI_CALIPER_AUTOMERGE = (true|false)
            
   Automatically merge attributes into a common context tree.
   Decreases the size of context records, but may increase the amount
   of metadata and reduce performance.

   Default: true

.. envvar:: CALI_SERVICES_ENABLE = (service1:service2:...)
            
   List of Caliper service modules to enable.

   Default: not set.

Many Caliper services define additional configuration variables. See
:doc:`services` for a list of Caliper services and their
configuration.

