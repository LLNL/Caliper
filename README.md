Caliper: A Performance Analysis Toolbox in a Library
==========================================

[![Github Actions](https://github.com/LLNL/Caliper/actions/workflows/cmake.yml/badge.svg)](https://github.com/LLNL/Caliper/actions)
[![Build Status](https://travis-ci.org/LLNL/Caliper.svg)](https://travis-ci.org/LLNL/Caliper)
[![Coverage](https://img.shields.io/codecov/c/github/LLNL/Caliper/master.svg)](https://codecov.io/gh/LLNL/Caliper)

Caliper is a performance instrumentation and profiling library for HPC
(high-performance computing) programs. It provides source-code annotation
APIs for marking regions of interest in C, C++, Fortran, and Python codes,
as well as measurement functionality for a wide range of runtime profiling,
event tracing, and performance monitoring use cases.

Caliper can generate simple human-readable reports or files for
performance data analysis frameworks like
[Hatchet](https://github.com/LLNL/hatchet)
and [Thicket](https://github.com/LLNL/thicket).
It can also generate detailed event traces for timeline visualizations with
[Perfetto](https://perfetto.dev) and the Google Chrome trace viewer.

Features include:

* Low-overhead source-code annotation API
* Recording program metadata for analyzing collections of runs
* Fully threadsafe implementation
* Support for parallel programming models like MPI, OpenMP, Kokkos, CUDA, and ROCm
* Event-based and sample-based performance measurements
* Trace and profile recording
* Connection to third-party tools, e.g. NVidia's NSight tools, AMD
  ROCProf, or Intel(R) VTune(tm)

Overview
------------------------------------------

Caliper is primarily a source-code instrumentation library. To use it, insert
Caliper instrumentation markers around source-code regions of interest in the
target program, like the C++ function and region markers in the example below:

```C++
#include <caliper/cali.h>

int get_answer() {
    CALI_CXX_MARK_FUNCTION;

    CALI_MARK_BEGIN("computing");
    int ret = 2 * 3 * 7;
    CALI_MARK_END("computing");
    return ret;
}
```

There are annotation APIs for C, C++, Fortran, and Python codes.
To take performance measurements, Caliper provides built-in profiling recipes for
a wide range of performance engineering use cases. Available functionality includes
MPI function and message profiling, CUDA and HIP API as well as GPU activity
profiling, loop profiling, call-path sampling, and much more.
As a simple example, the ``runtime-report`` recipe prints the time spent in the
annotated regions on screen:

    $ CALI_CONFIG=runtime-report ./answer
    Path          Time (E) Time (I) Time % (E) Time % (I)
    main          0.000072 0.000083  17.469875  20.188570
      get_answer  0.000008 0.000011   1.864844   2.718695
        computing 0.000004 0.000004   0.853851   0.853851

Aside from simple text reports, Caliper can generate machine-readable output in JSON
or its own custom .cali file format, which can be analyzed with the Caliper-provided
``cali-query`` tool and CalQL query language, or imported into Python analysis
scripts with the [caliper-reader](python/caliper-reader/) Python module.
In addition, Caliper can collect data for [Thicket](https://github.com/LLNL/thicket),
a Python-based toolkit for Exploratory Data Analysis of parallel performance data.

Documentation
------------------------------------------

Extensive documentation is available here:
https://software.llnl.gov/Caliper/

Usage examples of the C++, C, and Fortran annotation and ConfigManager
APIs are provided in the [examples](examples/apps) directory.

A basic tutorial is available here:
https://github.com/daboehme/caliper-tutorial

Building and installing
------------------------------------------

Caliper can be installed with the [spack](https://github.com/spack/spack)
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
