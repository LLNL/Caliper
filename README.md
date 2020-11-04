Caliper: A Performance Analysis Toolbox in a Library
==========================================

[![Build Status](https://travis-ci.org/LLNL/Caliper.svg)](https://travis-ci.org/LLNL/Caliper)
[![Coverage](https://img.shields.io/codecov/c/github/LLNL/Caliper/master.svg)](https://codecov.io/gh/LLNL/Caliper)

Caliper is a library to integrate performance profiling capabilities
into applications. To use Caliper, developers mark code regions of
interest using Caliper's annotation API. Applications can then enable
performance profiling at runtime with Caliper's configuration API.
Alternatively, you can configure Caliper through environment variables
or config files.

Caliper can be used for lightweight always-on profiling or advanced
performance engineering use cases, such as tracing, monitoring,
and auto-tuning. It is primarily aimed at HPC applications, but works
for any C/C++/Fortran program on Unix/Linux.

Features include:

* Low-overhead source-code annotation API
* Configuration API to control performance measurements from
  within an application
* Recording program metadata for analyzing collections of runs
* Flexible key:value data model to capture application-specific
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
     $ cmake -DCMAKE_INSTALL_PREFIX=<path to install location> ..
     $ make
     $ make install

Link Caliper to a program by adding `libcaliper`:

    g++ -o app app.o -L<path install location>/lib64 -lcaliper

There are many build flags to enable optional features, such as `-DWITH_MPI`
for MPI support.
See the "Build and install" section in the documentation for further
information.

Getting started
------------------------------------------

Typically, we integrate Caliper into a program by marking source-code
sections of interest with descriptive annotations. Performance profiling can
then be enabled through the Caliper ConfigManager API or environment
variables. Alternatively, third-party tools can connect to Caliper and access
information provided by the source-code annotations.

### Source-code annotations

Caliper's source-code annotation API allows you to mark source-code regions
of interest in your program. Much of Caliper's functionality depends on these
region annotations.

Caliper provides macros and functions for C, C++, and Fortran to mark
functions, loops, or sections of source-code. For example, use
`CALI_CXX_MARK_FUNCTION` to mark a function in C++:

```C++
#include <caliper/cali.h>

void foo()
{
    CALI_CXX_MARK_FUNCTION;
    // ...
}
```

You can mark arbitrary code regions with the `CALI_MARK_BEGIN` and
`CALI_MARK_END` macros or the corresponding `cali_begin_region()`
and `cali_end_region()` functions:

```C++
#include <caliper/cali.h>

// ...
CALI_MARK_BEGIN("my region");
// ...
CALI_MARK_END("my region");
```

The [cxx-example](examples/apps/cxx-example.cpp),
[c-example](examples/apps/c-example.c), and
[fortran-example](examples/apps/fortran-example.f) example apps show how to use
Caliper in C++, C, and Fortran, respectively.

### Recording performance data

With the source-code annotations in place, we can run performance measurements.
By default, Caliper does not record data - we have to activate performance
profiling at runtime.
An easy way to do this is to use one of Caliper's built-in measurement
configurations. For example, the `runtime-report` config prints out the time
spent in the annotated regions. You can activate built-in measurement
configurations with the ConfigManager API or with the `CALI_CONFIG`
environment variable. Let's try this on Caliper's cxx-example program:

    $ cd Caliper/build
    $ make cxx-example
    $ CALI_CONFIG=runtime-report ./examples/apps/cxx-example
    Path       Min time/rank Max time/rank Avg time/rank Time %
    main            0.000119      0.000119      0.000119  7.079120
      mainloop      0.000067      0.000067      0.000067  3.985723
        foo         0.000646      0.000646      0.000646 38.429506
      init          0.000017      0.000017      0.000017  1.011303

The runtime-report config works for MPI and non-MPI programs. It reports the
minimum, maximum, and average exclusive time (seconds) spent in each marked
code region across MPI ranks (the values are identical in non-MPI programs).

You can customize the report with additional options. Some options enable
additional Caliper functionality, such as profiling MPI and CUDA functions in
addition to the user-defined regions, or additional metrics like memory usage.
Other measurement configurations besides runtime-report include:

* loop-report: Print summary and time-series information for loops.
* mpi-report: Print time spent in MPI functions.
* callpath-sample-report: Print a time spent in functions using call-path sampling.
* event-trace: Record a trace of region enter/exit events in .cali format.
* hatchet-region-profile: Record a region time profile for processing with the
  [hatchet](https://github.com/LLNL/hatchet) library or cali-query.

See the "Builtin configurations" section in the documentation to learn more
about different configurations and their options.

You can also create entirely custom measurement configurations by selecting and
configuring Caliper services manually. See the "Manual configuration" section
in the documentation to learn more.

#### ConfigManager API

A distinctive Caliper feature is the ability to enable performance
measurements programmatically with the ConfigManager API. For example, we often
let users activate performance measurements with a command-line argument.

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

The `cxx-example` program uses the ConfigManager API to let users specify a
Caliper configuration with the `-P` command-line argument, e.g.
``-P runtime-report``:

    $ ./examples/apps/cxx-example -P runtime-report
    Path       Min time/rank Max time/rank Avg time/rank Time %
    main            0.000129      0.000129      0.000129  5.952930
      mainloop      0.000080      0.000080      0.000080  3.691740
        foo         0.000719      0.000719      0.000719 33.179511
      init          0.000021      0.000021      0.000021  0.969082

See the [Caliper documentation](https://llnl.github.io/Caliper/) for more
examples and the full API and configuration reference.

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
