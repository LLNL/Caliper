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
  module, e.g. with ``-Dadiak_DIR=<path-to-adiak>/lib/cmake/adiak``.

WITH_CUPTI
  Enable support for CUDA performance analysis (wrapping of driver/runtime API
  calls and CUDA activity tracing).

WITH_FORTRAN
  Build the Fortran wrappers.

WITH_PYTHON_BINDINGS
  Build the Python bindings.

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

WITH_PAPI_RDPMC
  Specify that PAPI is built to use :code:`rdpmc` by default for reading counters.

WITH_ROCTX
  Build adapters to forward Caliper annotations to AMD's roctx annotation API.

WITH_ROCTRACER
  Enable support for ROCm/HIP performance analysis (runtime API profiling and
  GPU activity tracing).

WITH_ROCPROFILER
  Enable roctx adapters and support for ROCm/HIP performance analysis with the
  rocprofiler-sdk API (available with ROCm 6.2 and higher).

WITH_SAMPLER
  Enable time-based sampling on Linux.

WITH_TOOLS
  Build Caliper's tools (i.e, cali-query and mpi-caliquery). Default: On.

WITH_VTUNE
  Build adapters to forward Caliper annotations to Intel's VTune annotation API.
  Set Intel ITT API installation dir in ``ITT_PREFIX``.

WITH_ARCH
  Specify the architecture for which you are building to enable
  architecture-specific functionality (e.g., topdown calculations).

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

In most cases, just link the "caliper" target.

Feature and build option overview
---------------------------------

The following table shows the features, recipes, and services that are enabled
with the given Caliper and spack build options.

+----------------------+---------------+---------------+---------------------------+--------------------+
| CMake option         | Default value | Spack option  | Enabled features/recipes  | Enabled services   |
+======================+===============+===============+===========================+====================+
| WITH_ADIAK           | False         | +adiak        | Import adiak metadata in  | adiak_import,      |
|                      |               |               | most config recipes       | adiak_export       |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_MPI             | False         | +mpi          | - mpi-report recipe       | mpi, mpireport     |
|                      |               |               | - profile.mpi,            |                    |
|                      |               |               |   mpi.message.count,      |                    |
|                      |               |               |   mpi.message.size        |                    |
|                      |               |               |   recipe options          |                    |
|                      |               |               | - Cross-process           |                    |
|                      |               |               |   aggregation             |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_PAPI            | False         | +papi         | - topdown.all,            | papi, topdown      |
|                      |               |               |   topdown.toplevel,       |                    |
|                      |               |               |   topdown-counters.*      |                    |
|                      |               |               |   recipe options for some |                    |
|                      |               |               |   x86 systems             |                    |
|                      |               |               | - PAPI counter collection |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_PAPI_RDPMC      | True          | not available | Topdown calculations      |                    | 
|                      |               | yet           | based on different        |                    | 
|                      |               |               | approaches to reading     |                    |
|                      |               |               | counters in PAPI          |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_LIBDW           | False         | +libdw        | - source.module,          | symbollookup       |
|                      |               |               |   source.function,        |                    |
|                      |               |               |   source.location         |                    |
|                      |               |               |   recipe options          |                    |
|                      |               |               | - Symbol name lookup      |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_LIBPFM          | False         | +libpfm       | PerfEvent counter         | libpfm             |
|                      |               |               | collection and precise    |                    |
|                      |               |               | event sampling            |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_LIBUNWIND       | False         | +libunwind    | - callpath option for     | callpath           |
|                      |               |               |   sample-report and       |                    |
|                      |               |               |   event-trace recipes     |                    |
|                      |               |               |   (requires libdw)        |                    |
|                      |               |               | - Call stack unwinding    |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_SAMPLER         | False         | +sampler      | - sample-report,          | sampler            |
|                      |               |               |   hatchet-sample-profile  |                    |
|                      |               |               |   recipes                 |                    |
|                      |               |               | - sampling option for     |                    |
|                      |               |               |   event-trace recipe      |                    |
|                      |               |               | - Linux sampling support  |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_CUPTI           | False         | +cuda         | - cuda-activity-report    | cupti, cuptitrace  |
|                      |               |               |   cuda-activity-profile   |                    |
|                      |               |               |   recipes                 |                    |
|                      |               |               | - profile.cuda,           |                    |
|                      |               |               |   cuda.gputime,           |                    |
|                      |               |               |   cuda.memcpy recipe      |                    |
|                      |               |               |   options                 |                    |
|                      |               |               | - CUDA API profiling      |                    |
|                      |               |               | - CUDA activity tracing   |                    |
+----------------------+---------------+               +---------------------------+--------------------+
| WITH_NVTX            | False         |               | - nvtx recipe             | nvtx               |
|                      |               |               | - Caliper-to-NVTX region  |                    |
|                      |               |               |   forwarding              |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_ROCTRACER       | False         | +rocm         | - rocm-activity-report,   | roctracer          |
|                      |               |               |   rocm-activity-profile   |                    |
|                      |               |               |   recipes                 |                    |
|                      |               |               | - profile.hip             |                    |
|                      |               |               |   rocm.gputime,           |                    |
|                      |               |               |   rocm.memcpy recipe      |                    |
|                      |               |               |   options                 |                    |
|                      |               |               | - ROCm/HIP API profiling  |                    |
|                      |               |               | - ROCm activity tracing   |                    |
+----------------------+---------------+               +---------------------------+--------------------+
| WITH_ROCTX           | False         |               | - roctx recipe            | roctx              |
|                      |               |               | - Caliper-to-ROCTX region |                    |
|                      |               |               |   forwarding              |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_ROCPROFILER     | False         | +rocprofiler  | - rocm-activity-report,   | rocprofiler, roctx |
|                      |               |               |   rocm-activity-profile,  |                    |
|                      |               |               | - profile.hip,            |                    |
|                      |               |               |   rocm.gputime,           |                    |
|                      |               |               |   rocm.memcpy options     |                    |
|                      |               |               | - Caliper-to-roctx region |                    |
|                      |               |               |   forwarding              |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_OMPT            | False         | not available | - openmp-report recipe    | ompt               |
|                      |               | yet           | - openmp.times,           |                    |
|                      |               |               |   openmp.threads,         |                    |
|                      |               |               |   openmp.efficiency       |                    |
|                      |               |               |   recipe options          |                    |
|                      |               |               | - OpenMP tools interface  |                    |
|                      |               |               |   support (CPU only, no   |                    |
|                      |               |               |   target offload)         |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_GOTCHA          | True on       | +gotcha       | - io.bytes.*,             | io, pthread,       |
|                      | Linux;        |               |   io.*.bandwidth,         | sysalloc           |
|                      | False         |               |   mem.highwatermark,      |                    |
|                      | otherwise     |               |   main_thread_only        |                    |
|                      |               |               |   recipe options          |                    |
|                      |               |               | - Use Gotcha for MPI      |                    |
|                      |               |               |   MPI function wrapping   |                    |
|                      |               |               |   instead of PMPI         |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_UMPIRE          | False         | not available | umpire.totals,            | umpire             |
|                      |               | yet           | umpire.allocators options |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_VARIORUM        | False         | +variorum     | Read variorum counters    | variorum           |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_PCP             | False         | not available | - mem.*.bandwidth,        | pcp, pcp.memory    |
|                      |               | yet           |   mem.*.bytes recipe      |                    |
|                      |               |               |   options on some LLNL    |                    |
|                      |               |               |   LC systems              |                    |
|                      |               |               | - Read Performance        |                    |
|                      |               |               |   CoPilot counters        |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_VTUNE           | False         | not available | Intel ITT API annotation  | vtune              |
|                      |               | yet           | forwarding                |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_CRAYPAT         | False         | not available | HPE CrayPAT API           | craypat            |
|                      |               | yet           | annotation forwarding     |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_KOKKOS          | True          | +kokkos       | Enable Kokkos tool API    | kokkostime,        |
|                      |               |               | bindings                  | kokkoslookup       |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_FORTRAN         | False         | +fortran      | Enable Fortran API        |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_PYTHON_BINDINGS | False         | +python       | Enable Python API         |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
| WITH_ARCH            | No default    | not available | Enable microarchitecture- |                    |
|                      |               | yet           | specific features         |                    |
+----------------------+---------------+---------------+---------------------------+--------------------+
