.. Caliper documentation master file, created by
   sphinx-quickstart on Wed Aug 12 16:55:34 2015.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Caliper: A Performance Analysis Toolbox in a Library
================================================================

Caliper is a program instrumentation and performance measurement
framework. It is designed as a performance analysis toolbox in a
library, allowing one to bake performance analysis capabilities
directly into applications and activate them at runtime.
Caliper is primarily aimed at HPC applications, but works for
any C/C++/Fortran program on Unix/Linux.

Caliper's data collection mechanisms and source-code annotation API
support a variety of performance engineering use cases, e.g.,
performance profiling, tracing, monitoring, and auto-tuning.

Features include:

* Low-overhead source-code annotation API
* Flexible key:value data model: capture application-specific
  features for performance analysis
* Fully threadsafe implementation, support for parallel programming
  models like MPI
* Synchronous (event-based) and asynchronous (sampling) performance
  data collection
* Trace and profile recording with flexible on-line and off-line
  data aggregation
* Connection to third-party tools, e.g. NVidia NVProf or
  Intel(R) VTune(tm)
* Measurement and profiling functionality such as timers, PAPI
  hardware counters, and Linux perf_events
* Memory allocation annotations: associate performance measurements
  with named memory regions

Caliper is available for download on
`GitHub <https://github.com/LLNL/Caliper>`_.

Contents
--------------------------------
.. toctree::
   :maxdepth: 1

   build
   workflow
   AnnotationAPI
   configuration
   services
   OutputFormats
   tools
   calql
   DeveloperGuide

Getting started
--------------------------------

Unlike traditional performance analysis tools, Caliper is designed to
be integrated directly within applications. This makes performance
analysis functionality accessible at any time and configurable at
runtime.

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
C++11-compatible Compiler. Clone Caliper from github and proceed
as follows:

.. code-block:: sh

     $ git clone https://github.com/LLNL/Caliper.git
     $ cd Caliper
     $ mkdir build && cd build
     $ cmake -DCMAKE_INSTALL_PREFIX=<path to install location> \
         -DCMAKE_C_COMPILER=<path to c-compiler> \
         -DCMAKE_CXX_COMPILER=<path to c++-compiler> \
         ..
     $ make
     $ make install

See :doc:`build` for more details.

Source-code annotations
................................

Caliper's source-code annotation API fulfills two purposes: First, it
lets us associate performance measurements with user-defined,
high-level context information. Second, we can trigger user-defined
actions at the instrumentation points, e.g. to measure the time spent
in individual regions. Measurement actions can be defined at runtime
and are disabled by default; generally, the source-code annotations
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
        CALI_MARK_BEGIN("initialization");
        int count = 4;
        double t = 0.0, delta_t = 1e-6;
        CALI_MARK_END("initialization");

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

See the :doc:`AnnotationAPI` chapter for a complete reference of the C,
C++, and Fortran annotation APIs.

Linking the Caliper library
................................

To use Caliper, add annotation statements to your program and link it
against the Caliper library. Programs must be linked with the Caliper
runtime (libcaliper.so), as shown in the example link command: ::

    g++ -o app app.o -L<path to caliper installation>/lib64 -lcaliper

Runtime configuration
................................

Caliper's performance measurement and data collection functionality
must be enabled and configured at runtime through Caliper's
configuration API, configuration files, or environment variables. By
default, Caliper will keep track of the current Caliper context
provided by the annotation API calls (allowing third-party tools to
access program context information), but won't run any performance
measurement or data recording on its own.

Generally, collecting performance data with Caliper requires selecting
a combination of Caliper *services* that implement specific
functionality and configuring them for the task at hand. However, for
some common scenarios, Caliper provides a set of pre-defined
configuration profiles. These profiles can be activated with the
``CALI_CONFIG_PROFILE`` environment variable. For example, the
``runtime-report`` configuration profile prints the total time (in
microseconds) spent in each code path based on the nesting of
annotated code regions: ::

    $ CALI_CONFIG_PROFILE=runtime-report ./examples/apps/cali-basic-annotations
    Path          sum#time.duration
    main                  20.000000
      main loop            8.000000
      init                10.000000

The example shows Caliper output for the ``runtime-report``
configuration profile for the source-code annotation example above.

As another example, the ``serial-trace`` configuration profile
configures Caliper to record an event trace of each annotation event: ::

    $ CALI_CONFIG_PROFILE=serial-trace ./examples/apps/cali-basic-annotations
    == CALIPER: Registered event trigger service
    == CALIPER: Registered recorder service
    == CALIPER: Registered timestamp service
    == CALIPER: Registered trace service
    == CALIPER: Initialized
    == CALIPER: Flushing Caliper data
    == CALIPER: Trace: Flushed 14 snapshots.
    == CALIPER: Recorder: Wrote 71 records.

The trace data is stored in a ``.cali`` file in a text-based
Caliper-specific file format. Use the ``cali-query`` tool to filter,
aggregate, or print the recorded data. Here, we use ``cali-query`` to
print the recorded trace data in a human-readable json format:

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

The :doc:`configuration` section demonstrates the set up of Caliper runtime
configurations through environment variables or configuration files.

Where to go from here
................................

Caliper's performance measurement and data collection functionality is
provided by independent building blocks called *services*, each
implementing specific functionality (e.g., tracing, I/O, timing,
report formatting, sampling, etc.). The services can be enabled at
runtime in any combination. This makes Caliper highly flexible, but
the runtime configuration can be complex. The :doc:`workflow` section
explains more about Caliper's internal dataflow and how services
interact.

Index
--------------------------------

* :ref:`genindex`
