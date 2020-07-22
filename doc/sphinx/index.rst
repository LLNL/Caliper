.. Caliper documentation master file, created by
   sphinx-quickstart on Wed Aug 12 16:55:34 2015.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Caliper: A Performance Analysis Toolbox in a Library
================================================================

Caliper is a program instrumentation and performance measurement
framework. It is a performance-analysis toolbox in a
library, allowing one to bake performance analysis capabilities
directly into applications and activate them at runtime.
Caliper is primarily aimed at HPC applications, but works for
any C/C++/Fortran program on Unix or Linux.

Caliper's data-collection mechanisms and source-code annotation API
support a variety of performance-engineering use cases, e.g.,
performance profiling, tracing, monitoring, and auto-tuning.

Features include:

* Low-overhead source-code annotation API
* Configuration API to control performance measurements from
  within an application
* Flexible key:value data model -- capture application-specific
  features for performance analysis
* Fully threadsafe implementation, support for parallel programming
  models like MPI
* Trace and profile recording with flexible online and offline
  data aggregation
* Connection to third-party tools, e.g., NVidia NVProf and
  Intel(R) VTune(tm)
* Measurement and profiling functionality, such as timers, PAPI
  hardware counters, and Linux perf_events
* Memory-allocation annotations: associate performance measurements
  with named memory regions

Caliper is available for download on `GitHub
<https://github.com/LLNL/Caliper>`_.

Contents
--------------------------------
.. toctree::
   :maxdepth: 1

   build
   AnnotationAPI
   ConfigManagerAPI
   FortranSupport
   configuration
   workflow
   services
   ThirdPartyTools
   calql
   OutputFormats
   tools
   DeveloperGuide

Getting started
--------------------------------

Unlike traditional performance-analysis tools, Caliper integrates directly
with applications. This makes performance-analysis functionality accessible
at any time and configurable at runtime through a configuration API.

Typically, we integrate Caliper into a program by marking source-code
sections of interest with descriptive annotations. Performance
measurements, trace or profile collection, and reporting functionality
can then be enabled with runtime configuration options. Alternatively,
third-party tools can connect to Caliper and access information
provided by the source-code annotations.

Building and installing
................................

Download Caliper from `GitHub <https://github.com/LLNL/Caliper>`_.

Building and installing Caliper requires cmake 3.1+ and a current
C++11-compatible compiler. See :doc:`build` for details.

You can also install Caliper through `Spack <https://github.com/spack/spack>`_.

To use Caliper, add annotation statements to your program and link it
against the Caliper library. Programs must be linked with the Caliper
runtime (libcaliper.so), as shown in the example link command:

.. code-block:: sh

    g++ -o app app.o -L<path to caliper installation>/lib64 -lcaliper

Source-code annotations
................................

Caliper source-code annotations let us associate performance measurements
with user-defined, high-level context information. We can also
trigger user-defined actions at the instrumentation points, e.g. to measure
the time spent in annotated regions. Measurement actions can be defined at
runtime and are disabled by default; generally, the source-code annotations
are lightweight enough to be left in production code.

The annotation APIs are available for C, C++, and Fortran. There are
high-level annotation macros for common scenarios such as marking
functions, loops, or sections of source-code. In addition, users can
export arbitrary key:value pairs to express application-specific
concepts.

The following example marks "initialization" and "main loop" phases in
a C++ code, and exports the main loop's current iteration counter
using the high-level annotation macros:

.. code-block:: c++

    #include <caliper/cali.h>

    int main(int argc, char* argv[])
    {
        // Mark this function
        CALI_CXX_MARK_FUNCTION;

        // Mark the "intialization" phase
        CALI_MARK_BEGIN("init");
        int count = 4;
        double t = 0.0, delta_t = 1e-6;
        CALI_MARK_END("init");

        // Mark the loop
        CALI_CXX_MARK_LOOP_BEGIN(mainloop, "main loop");

        for (int i = 0; i < count; ++i) {
            // Mark each loop iteration
            CALI_CXX_MARK_LOOP_ITERATION(mainloop, i);

            // A Caliper snapshot taken at this point will contain
            // { "function"="main", "loop"="main loop", "iteration#main loop"=<i> }

            // ...
        }

        CALI_CXX_MARK_LOOP_END(mainloop);
    }

See the :doc:`AnnotationAPI` chapter for a complete reference to the C,
C++, and Fortran annotation APIs.

Recording Performance Data
................................

Performance measuremens can be controlled most conveniently via the
ConfigManager API. The ConfigManager covers many common use cases,
such as recording traces or printing time reports. For custom analysis
tasks or when not using the ConfigManager API, Caliper can be configured
manually by setting configuration variables via the environment or
in config files.

ConfigManager API
###############################

With the C++ ConfigManager API, built-in performance measurement and
reporting configurations can be activated within a program using a short
configuration string. This configuration string can be hard-coded in the
program or provided by the user in some form, e.g. as a command-line
parameter or in the programs's configuration file.

To use the ConfigManager API, create a `cali::ConfigManager` object, add a
configuration string with `add()`, start the requested configuration
channels with `start()`, and trigger output with `flush()`:

.. code-block:: c++

    #include <caliper/cali-manager.h>
    // ...
    cali::ConfigManager mgr;
    mgr.add("runtime-report");
    // ...
    mgr.start(); // start requested performance measurement channels
    // ... (program execution)
    mgr.flush(); // write performance results

A complete code example where users can provide a configuration string
on the command line is [here](examples/apps/cxx-example.cpp). Built-in
configs include

runtime-report
  Prints a time profile for annotated code regions.

event-trace
  Records a trace of region begin/end events.

See :doc:`ConfigManagerAPI` to learn more.

Manual runtime configuration
###############################################

The ConfigManager API is not required to run Caliper - performance
measurements can also be configured with environment variables or
a config file.
For starters, there are a set of pre-defined configuration
profiles that can be activated with the `CALI_CONFIG_PROFILE`.
For example, the `runtime-report` configuration profile prints
the total time spent in each code path based on the nesting of
annotated code regions: ::

  $ CALI_CONFIG_PROFILE=runtime-report ./examples/apps/cxx-example
  Path          Inclusive time Exclusive time Time %
  main                0.000099       0.000070 5.751849
    main loop         0.000013       0.000013 1.068200
    init              0.000016       0.000016 1.314708

For complete control, users can select and configure Caliper
*services* (:doc:`services`) manually. See :doc:`configuration`
to learn more. For example, this configuration records an event
trace for each annotation event: ::

    $ CALI_SERVICES_ENABLE=event,recorder,timestamp,trace ./examples/apps/cxx-example

Post-processing
###############################################

The trace data is stored in a ``.cali`` file in a text-based,
Caliper-specific file format. Use the ``cali-query`` tool to filter,
aggregate, or print the recorded data. Here we use ``cali-query`` to
print the recorded trace data as json text:

.. code-block:: sh

    $ ls *.cali
    171120-181836_40337_7LOlCN5RchWV.cali
    $ cali-query 171120-181836_40337_7LOlCN5RchWV.cali -q "SELECT * FORMAT json(pretty)"
    [
    {
            "event.begin#function":"main"
    },
    {
            "event.begin#annotation":"init",
            "function":"main"
    },
    {
            "event.end#annotation":"init",
            "annotation":"init",
            "function":"main",
            "time.inclusive.duration":14
    },
    {
            "event.begin#loop":"main loop",
            "function":"main"
    },
    {
            "loop":"main loop",
            "function":"main",
            "event.begin#iteration#main loop":0
    },
    ...

Where to go from here
................................

Caliper's performance-measurement and data-collection functionality is
provided by independent building blocks called *services*, each
implementing a specific functionality (e.g., tracing, I/O, timing,
report formatting, or sampling). The services can be enabled at
runtime in any combination. This makes Caliper highly flexible, but
the runtime configuration can be complex. The :doc:`workflow` section
explains more about Caliper's internal dataflow and how services
interact.

Index
--------------------------------

* :ref:`genindex`
