Building Caliper
================================

Building and installing Caliper requires cmake, a current C++11-compatible
compiler, and a Python interpreter. To build Caliper manually, clone it from the
`github repository <https://github.com/LLNL/Caliper>`_.
Next, configure and build Caliper, e.g.:

.. code-block:: sh

     $ cd <path to caliper root directory>
     $ mkdir build && cd build
     $ cmake -DCMAKE_INSTALL_PREFIX=<path to install location> ..
     $ make
     $ make install

This is a minimal configuration, see below for additional CMake configuration flags.

CMake Flags
................................

Configure Caliper using the usual CMake flags to select the C and C++ compiler,
build type, and install prefix (CMAKE_C_COMPILER, CMAKE_CXX_COMPILER,
CMAKE_BUILD_TYPE, and CMAKE_INSTALL_PREFIX).
In addition, Caliper supports the following CMake flags:

BUILD_DOCS
  Build documentation.

BUILD_SHARED_LIBS
  Build shared or static library. Default: On (build shared library).

BUILD_TESTING
  Build unit tests.

WITH_ADIAK
  Enable support for recording program metadata with the
  `Adiak <https://github.com/LLNL/Adiak>`_ library. Point CMake to adiak CMake
  module, e.g. with ``-Dadiak_PREFIX=<path-to-adiak>/lib/cmake/adiak``.

WITH_CUPTI
  Enable support for CUDA performance analysis (wrapping of driver/runtime API
  calls and CUDA activity tracing).

WITH_FORTRAN
  Build the Fortran wrappers.

WITH_GOTCHA
  Enable Gotcha support. Allows pthread, IO, and malloc/free tracking, and
  enables dynamic wrapping of MPI functions.
  If Gotcha is disabled, MPI wrappers use the PMPI interface.
  Requires Linux.

WITH_LIBDW
  Enables libdw support for DWARF symbol lookup. Required for most
  sampling-based configurations.

WITH_LIBPFM
  Enable Linux perf_event sampling. Set libpfm installation dir
  in LIBPFM_INSTALL.

WITH_LIBUNWIND
  Enables libunwind support for call-path unwinding.

WITH_MPI
  Build with MPI support.

WITH_NVTX
  Build adapters to forward Caliper annotations to NVidia's nvtx annotation API.
  Set CUDA_TOOLKIT_ROOT_DIR to the CUDA installation.

WITH_OMPT
  Build with support for the OpenMP tools interface.

WITH_PAPI
  Enable PAPI support. Set PAPI installation dir in PAPI_PREFIX.

WITH_ROCTX
  Build adapters to forward Caliper annotations to AMD's roctx annotation API.

WITH_ROCTRACER
  Enable support for ROCm/HIP performance analysis (runtime API profiling and
  GPU activity tracing).

WITH_SAMPLER
  Enable time-based sampling on Linux.

WITH_TOOLS
  Build Caliper's tools (i.e, cali-query and mpi-caliquery). Default: On.

WITH_VTUNE
  Build adapters to forward Caliper annotations to Intel's VTune annotation API.
  Set Intel ITT API installation dir in ``ITT_PREFIX``.

All options are off by default. On Linux, Gotcha is enabled by default.

Linking Caliper programs
--------------------------------

Typically, all that is needed to create a Caliper-enabled program is
to link it with the Caliper runtime library, which resides in
`libcaliper.so`. An example link command for a C++ program built
with g++ could look like this: ::

  CALIPER_DIR = /path/to/caliper/installation

  g++ -o target-program $(OBJECTS) -L$(CALIPER_DIR)/lib64 -lcaliper

Using Caliper in CMake projects
................................

Caliper creates a CMake package file (caliper-config.cmake) and
installs it in <caliper-installation-dir>/lib64/cmake/caliper. The
package file defines Caliper's include directories and exports targets
for the Caliper libraries. Projects using CMake can use find_package()
and target_link_libraries() to integrate Caliper as a dependency.

This example CMake code builds a program which depends on Caliper: ::

  find_package(caliper)
  add_executable(MyExample MyExample.cpp)
  target_link_libraries(MyExample PRIVATE caliper)

When configuring the target program, point CMake to the desired
Caliper installation with `caliper_DIR`: ::

  cmake -Dcaliper_DIR=<caliper-installation-dir>/share/cmake/caliper ..

The CMake package defines the following variables and targets:

+----------------------------+------------------------------------------+
| ${caliper_INCLUDE_DIR}     | Caliper include directory (variable)     |
+----------------------------+------------------------------------------+
| caliper                    | The Caliper runtime library (target)     |
+----------------------------+------------------------------------------+
| caliper-serial             | Caliper runtime library without MPI      |
|                            | dependencies (target)                    |
+----------------------------+------------------------------------------+
| caliper-tools-util         | Utilities for caliper tools (target)     |
+----------------------------+------------------------------------------+

In most cases, just link the "caliper" target.

Feature and build option overview
---------------------------------

The following table shows the features, recipes, and services that are enabled
with the given Caliper and spack build options.

+----------------+---------------+---------------------------+--------------------+
| CMake option   | Spack option  | Enabled features/recipes  | Enabled services   |
+----------------+---------------+---------------------------+--------------------+
| WITH_ADIAK     | +adiak        | Import adiak metadata in  | adiak_import,      |
|                |               | most config recipes       | adiak_export       |
+----------------+---------------+---------------------------+--------------------+
| WITH_MPI       | +mpi          | mpi-report recipe;        | mpi, mpireport     |
|                |               | profile.mpi,              |                    |
|                |               | mpi.message.count,        |                    |
|                |               | mpi.message.size          |                    |
|                |               | recipe options;           |                    |
|                |               | Cross-process aggregation |                    |
+----------------+---------------+---------------------------+--------------------+
| WITH_PAPI      | +papi         | topdown.all,              | papi, topdown      |
|                |               | topdown.toplevel,         |                    |
|                |               | topdown-counters.all,     |                    |
|                |               | topdown-counters.toplevel |                    |
|                |               | recipe options for some   |                    |
|                |               | x86 systems;              |                    |
|                |               | PAPI counter collection   |                    |
+----------------+---------------+---------------------------+--------------------+
| WITH_LIBDW     | +libdw        | source.module,            | symbollookup       |
|                |               | source.function,          |                    |
|                |               | source.location recipe    |                    |
|                |               | options;                  |                    |
|                |               | Symbol name lookup        |                    |
+----------------+---------------+---------------------------+--------------------+
| WITH_LIBPFM    | +libpfm       | PerfEvent counter         | libpfm             |
|                |               | collection and precise    |                    |
|                |               | event sampling            |                    |
+----------------+---------------+---------------------------+--------------------+
| WITH_LIBUNWIND | +libunwind    | callpath option for       | callpath           |
|                |               | sample-report and         |                    |
|                |               | event-trace recipes (also |                    |
|                |               | requires libdw);          |                    |
|                |               | Stack unwinding           |                    |
+----------------+---------------+---------------------------+--------------------+
| WITH_SAMPLER   | +sampler      | sample-report,            | sampler            |
|                |               | hatchet-sample-profile    |                    |
|                |               | recipes; sampling option  |                    |
|                |               | for event-trace           |                    |
+----------------+---------------+---------------------------+--------------------+
| WITH_CUPTI     | +cuda         | cuda-activity-report,     | cupti, cuptitrace  |
|                |               | cuda-activity-profile     |                    |
|                |               | recipes; profile.cuda,    |                    |
|                |               | cuda.gputime,             |                    |
|                |               | cuda.memcpy recipe        |                    |
|                |               | options                   |                    |
+----------------+               +---------------------------+--------------------+
| WITH_NVTX      |               | nvtx recipe;              | nvtx               |
|                |               | Caliper-to-NVTX region    |                    |
|                |               | forwarding                |                    |
+----------------+---------------+---------------------------+--------------------+
| WITH_ROCTRACER | +rocm         | rocm-activity-report,     | roctracer          |
|                |               | rocm-activity-profile     |                    |
|                |               | recipes; profile.hip,     |                    |
|                |               | rocm.gputime,             |                    |
|                |               | rocm.memcpy options       |                    |
+----------------+               +---------------------------+--------------------+
| WITH_ROCTX     |               | roctx recipe;             | roctx              |
|                |               | Caliper-to-ROCTX region   |                    |
|                |               | forwarding                |                    |
+----------------+---------------+---------------------------+--------------------+
| WITH_OMPT      | not available | openmp-report recipe;     | ompt               |
|                | yet           | openmp.times,             |                    |
|                |               | openmp.threads,           |                    |
|                |               | openmp.efficiency recipe  |                    |
|                |               | options;                  |                    |
|                |               | OpenMP tool interface     |                    |
|                |               | support (CPU only, no     |                    |
|                |               | target offload)           |                    |
+----------------+---------------+---------------------------+--------------------+
| WITH_GOTCHA    | +gotcha       | io.bytes.*,               | io, pthread,       |
|                |               | io.*.bandwidth,           | sysalloc           |
|                |               | mem.highwatermark,        |                    |
|                |               | main_thread_only options; |                    |
|                |               | Use Gotcha                |                    |
|                |               | for MPI function wrapping |                    |
|                |               | instead of PMPI           |                    |
+----------------+---------------+---------------------------+--------------------+
| WITH_UMPIRE    | not available | umpire.totals,            | umpire             |
|                | yet           | umpire.allocators options |                    |
+----------------+---------------+---------------------------+--------------------+
| WITH_VARIORUM  | +variorum     | Read variorum counters    | variorum           |
+----------------+---------------+---------------------------+--------------------+
| WITH_PCP       | not available | mem.*.bandwidth,          | pcp, pcp.memory    |
|                | yet           | mem.*.bytes recipe        |                    |
|                |               | options on some LLNL LC   |                    |
|                |               | systems;                  |                    |
|                |               | Read Performance CoPilot  |                    |
|                |               | counters                  |                    |
+----------------+---------------+---------------------------+--------------------+
| WITH_VTUNE     | not available | Intel ITT API annotation  | vtune              |
|                | yet           | forwarding                |                    |
+----------------+---------------+---------------------------+--------------------+
| WITH_CRAYPAT   | not available | HPE CrayPAT API           | craypat            |
|                | yet           | annotation forwarding     |                    |
+----------------+---------------+---------------------------+--------------------+
| WITH_KOKKOS    | +kokkos       | Enable Kokkos tool API    | kokkostime,        |
|                |               | bindings                  | kokkoslookup       |
+----------------+---------------+---------------------------+--------------------+
| WITH_FORTRAN   | +fortran      | Enable Fortran API        |                    |
+----------------+---------------+---------------------------+--------------------+
