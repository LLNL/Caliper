Caliper: A Performance Analysis Toolbox in a Library
==========================================

[![Github Actions](https://github.com/LLNL/Caliper/actions/workflows/cmake.yml/badge.svg)](https://github.com/LLNL/Caliper/actions)
[![Build Status](https://travis-ci.org/LLNL/Caliper.svg)](https://travis-ci.org/LLNL/Caliper)
[![Coverage](https://img.shields.io/codecov/c/github/LLNL/Caliper/master.svg)](https://codecov.io/gh/LLNL/Caliper)

Caliper is a performance instrumentation and profiling library for HPC
(high-performance computing) programs. It provides source-code annotation
APIs for marking regions of interest in C, C++, Fortran, and Python codes,
as well as performance measurement functionality for a wide range of use cases,
such as runtime profiling, event tracing, and performance monitoring.

Caliper can generate simple human-readable reports or machine-readable
JSON or .cali files for automated data processing with user-provided scripts
or analysis frameworks like [Hatchet](https://github.com/LLNL/hatchet)
and [Thicket](https://github.com/LLNL/thicket).
It can also generate detailed event traces for timeline visualizations with
[Perfetto](https://perfetto.dev) and the Google Chrome trace viewer.

Features include:

* Low-overhead source-code annotation API
* Configuration API to control performance measurements from
  within an application
* Recording program metadata for analyzing collections of runs
* Flexible key:value data model to capture application-specific
  features for performance analysis
* Fully threadsafe implementation, support for parallel programming
  models like MPI, OpenMP, Kokkos, CUDA, and ROCm
* Event-based and sample-based performance measurements
* Trace and profile recording
* Connection to third-party tools, e.g. NVidia's NSight tools, AMD
  ROCProf, or Intel(R) VTune(tm)

Documentation
------------------------------------------

Extensive documentation is available here:
https://software.llnl.gov/Caliper/

Usage examples of the C++, C, and Fortran annotation and ConfigManager
APIs are provided in the [examples](examples/apps) directory.

See the "Getting started" section below for a brief tutorial.

Building and installing
------------------------------------------

You can install Caliper with the [spack](https://github.com/spack/spack)
package manager:

    $ spack install caliper

Building Caliper manually requires cmake 3.12+ and a C++11-compatible
Compiler. Clone Caliper from github and proceed as follows:

    $ git clone https://github.com/LLNL/Caliper.git
    $ cd Caliper
    $ mkdir build && cd build
    $ cmake -DCMAKE_INSTALL_PREFIX=<path to install location> ..
    $ make
    $ make install

Link Caliper to a program by adding `libcaliper`:

    $ g++ -o app app.o -L<path install location>/lib64 -lcaliper

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
recipes. For example, the `runtime-report` config prints out the time
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
* sample-report: Print time spent in functions using sampling.
* event-trace: Record a trace of region enter/exit events in .cali format.
* hatchet-region-profile: Record a region time profile for processing with
  [Hatchet](https://github.com/LLNL/hatchet) or cali-query.

See the "Builtin configurations" section in the documentation to learn more
about different configurations and their options.

You can also create entirely custom measurement configurations by selecting and
configuring Caliper services manually. See the "Manual configuration" section
in the documentation to learn more.

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

On GitHub, you can copy this citation in APA or BibTeX format via the
"Cite this repository" button. Or, see the comments in CITATION.cff
for the raw BibTeX.

Release
------------------------------------------

Caliper is released under a BSD 3-clause license. See [LICENSE](LICENSE) for details.

``LLNL-CODE-678900``
