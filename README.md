Caliper: A Performance Analysis Toolbox in a Library
==========================================

[![Build Status](https://travis-ci.org/LLNL/Caliper.svg)](https://travis-ci.org/LLNL/Caliper)
[![Coverage](https://img.shields.io/codecov/c/github/LLNL/Caliper/master.svg)](https://codecov.io/gh/LLNL/Caliper)

Caliper is a program instrumentation and performance measurement
framework. It is a performance analysis toolbox in a library,
allowing one to bake performance analysis capabilities
directly into applications and activate them at runtime.

Caliper can be used for lightweight always-on profiling or advanced
performance engineering use cases, such as tracing, monitoring,
and auto-tuning. It is primarily aimed at HPC applications, but works
for any C/C++/Fortran program on Unix/Linux.

Features include:

* Low-overhead source-code annotation API
* Configuration API to control performance measurements from
  within an application
* Flexible key:value data model: capture application-specific
  features for performance analysis
* Fully threadsafe implementation, support for parallel programming
  models like MPI
* Synchronous (event-based) and asynchronous (sampling) performance
  data collection
* Trace and profile recording
* Connection to third-party tools, e.g. NVidia NVProf or
  Intel(R) VTune(tm)
* Measurement and profiling functionality such as timers, PAPI
  hardware counters, and Linux perf_events
* Memory allocation annotations: associate performance measurements
  with named memory regions

Documentation
------------------------------------------

Extensive documentation is available here:
https://llnl.github.io/Caliper/

Usage examples of the C++, C, and Fortran annotation and ConfigManager
APIs are provided in the [examples](examples/apps) directory.

See the "Getting started" section below for a brief tutorial.

Building and installing
------------------------------------------

Building and installing Caliper requires cmake 3.1+ and a current
C++11-compatible Compiler. Clone Caliper from github and proceed
as follows:

     $ git clone https://github.com/LLNL/Caliper.git
     $ cd Caliper
     $ mkdir build && cd build
     $ cmake -DCMAKE_INSTALL_PREFIX=<path to install location> \
         -DCMAKE_C_COMPILER=<path to c-compiler> \
         -DCMAKE_CXX_COMPILER=<path to c++-compiler> \
         ..
     $ make
     $ make install

See the "Build and install" section in the documentation for further
information.

Getting started
------------------------------------------

Typically, we integrate Caliper into a program by marking source-code
sections of interest with descriptive annotations. Performance
measurements, trace or profile collection, and reporting functionality
can then be enabled with runtime configuration options. Alternatively,
third-party tools can connect to Caliper and access information
provided by the source-code annotations.

### Source-code annotations

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

```C++
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
```

### Linking the Caliper library

To use Caliper, add annotation statements to your program and link it
against the Caliper library. Programs must be linked with the Caliper
runtime (libcaliper.so), as shown in the example link command:

    g++ -o app app.o -L<path to caliper installation>/lib64 -lcaliper

### Recording performance data

Performance measuremens can be controlled most conveniently via the
ConfigManager API. The ConfigManager covers many common use cases,
such as recording traces or printing time reports. For custom analysis
tasks or when not using the ConfigManager API, Caliper can be configured
manually by setting configuration variables via the environment or
in config files.

#### ConfigManager API

With the C++ ConfigManager API, built-in performance measurement and
reporting configurations can be activated within a program using a short
configuration string. This configuration string can be hard-coded in the
program or provided by the user in some form, e.g. as a command-line
parameter or in the programs's configuration file.

To use the ConfigManager API, create a `cali::ConfigManager` object, add a
configuration string with `add()`, start the requested configuration
channels with `start()`, and trigger output with `flush()`:

```C++
#include <caliper/cali-manager.h>
// ...
cali::ConfigManager mgr;
mgr.add("runtime-report");
// ...
mgr.start(); // start requested performance measurement channels
// ... (program execution)
mgr.flush(); // write performance results
```

A complete code example where users can provide a configuration string
on the command line is [here](examples/apps/cxx-example.cpp). Built-in
configs include

* runtime-report: Prints a time profile for annotated code regions.
* event-trace: Records a trace of region begin/end events.

Complete documentation on the ConfigManager configurations can be found
[here](doc/ConfigManagerAPI.md).

#### Manual configuration through environment variables

The ConfigManager API is not required to run Caliper - performance
measurements can also be configured with environment variables or a config
file. For starters, there are a set of pre-defined configuration
profiles that can be activated with the
`CALI_CONFIG_PROFILE` environment variable. For example, the
`runtime-report` configuration profile prints the total time (in
microseconds) spent in each code path based on the nesting of
annotated code regions:

    $ CALI_CONFIG_PROFILE=runtime-report ./examples/apps/cxx-example
    Path         Inclusive time (usec) Exclusive time (usec) Time %
    main                     38.000000             20.000000   52.6
      main loop               8.000000              8.000000   21.1
      init                   10.000000             10.000000   26.3

The example shows Caliper output for the `runtime-report`
configuration profile for the source-code annotation example above.

For complete control, users can select and configure Caliper *services*
manually. For example, this configuration records an event trace
recording all enter and leave events for annotated code regions:

    $ CALI_SERVICES_ENABLE=event,recorder,timestamp,trace ./examples/apps/cxx-example
    == CALIPER: Registered event trigger service
    == CALIPER: Registered recorder service
    == CALIPER: Registered timestamp service
    == CALIPER: Registered trace service
    == CALIPER: Initialized
    == CALIPER: Flushing Caliper data
    == CALIPER: Trace: Flushed 14 snapshots.
    == CALIPER: Recorder: Wrote 71 records.

The trace data is stored in a `.cali` file in a text-based
Caliper-specific file format. Use the `cali-query` tool to filter,
aggregate, or print the recorded data. Here, we use `cali-query` to
print the recorded trace data in a human-readable json format:

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
    ...

More information can be found in the [Caliper documentation](https://llnl.github.io/Caliper/).

Authors
------------------------------------------

Caliper was created by [David Boehme](https://github.com/daboehme), boehme3@llnl.gov.

A complete [list of contributors](https://github.com/LLNL/Caliper/graphs/contributors) is available on GitHub.

Major contributors include:

* [Alfredo Gimenez](https://github.com/alfredo-gimenez) (libpfm support, memory allocation tracking)
* [David Poliakoff](https://github.com/DavidPoliakoff) (GOTCHA support)

### Citing Caliper

To reference Caliper in a publication, please cite the following paper:

* David Boehme, Todd Gamblin, David Beckingsale, Peer-Timo Bremer,
  Alfredo Gimenez, Matthew LeGendre, Olga Pearce, and Martin
  Schulz.
  [**Caliper: Performance Introspection for HPC Software Stacks.**](http://ieeexplore.ieee.org/abstract/document/7877125/)
  In *Supercomputing 2016 (SC16)*, Salt Lake City, Utah,
  November 13-18, 2016. LLNL-CONF-699263.

Release
------------------------------------------

Caliper is released under a BSD 3-clause license. See [LICENSE](LICENSE) for details.

``LLNL-CODE-678900``
