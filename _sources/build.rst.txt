Build and link
================================

Building and installing Caliper requires cmake, a current C++11-compatible
compiler, and a Python interpreter.

Building Caliper
--------------------------------

To build Caliper manually, clone it from the
`github repository <https://github.com/LLNL/Caliper>`_.

Next, configure and build Caliper, e.g.:

.. code-block:: sh

     $ cd <path to caliper root directory>
     $ mkdir build && cd build
     $ cmake -DCMAKE_INSTALL_PREFIX=<path to install location> \
         -DCMAKE_C_COMPILER=<path to c-compiler> \
         -DCMAKE_CXX_COMPILER=<path to c++-compiler> \
         ..
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
  Enable Adiak support. Point CMake to adiak CMake module, e.g. with
  ``-Dadiak_PREFIX=<path-to-adiak>/lib/cmake/adiak``.

WITH_CALLPATH
  Enables callpath unwinding support. Requires libunwind.

WITH_CUPTI
  Enable support for CUDA performance analysis (wrapping of driver/runtime API
  calls and CUDA activity tracing). Set CUpti installation dir
  with CUPTI_PREFIX.

WITH_DYNINST
  Enable address-to-symbol lookup using Dyninst.
  Point CMake to the Dyninst CMake module, e.g. with
  ``-DDyninst_PREFIX=<path-to-dyninst>/lib/cmake/Dyninst``.

WITH_FORTRAN
  Build the Fortran wrappers.

WITH_GOTCHA
  Enable Gotcha support. Allows pthread, IO, and malloc/free tracking, and
  enables dynamic wrapping of MPI functions.
  If Gotcha is disabled, MPI wrappers use the PMPI interface.
  Requires Linux.

WITH_LIBPFM
  Enable Linux perf_event sampling. Set libpfm installation dir
  in LIBPFM_INSTALL.

WITH_MPI
  Build with MPI support.

WITH_NVPROF
  Build adapters to forward Caliper annotations to NVidia's nvtx annotation API.
  Set CUDA_TOOLKIT_ROOT_DIR to the CUDA installation.

WITH_PAPI
  Enable PAPI support. Set PAPI installation dir in PAPI_PREFIX.

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
installs it in <caliper-installation-dir>/share/cmake/caliper. The
package file defines Caliper's include directories and exports targets
for the Caliper libraries. Projects using CMake can use find_package()
and target_link_libraries() to integrate Caliper as a dependency.

This example CMakeLists.txt builds a program which depends on Caliper: ::

  cmake_minimum_required(VERSION 3.0)

  project(MyExample CXX)

  find_package(caliper)

  add_executable(MyExample MyExample.cpp)

  target_include_directories(MyExample
    PRIVATE ${caliper_INCLUDE_DIR})

  target_link_libraries(MyExample
    caliper)

When configuring the target program, point CMake to the desired
Caliper installation with `caliper_DIR`: ::

  cmake -Dcaliper_DIR=<caliper-installation-dir>/share/cmake/caliper ..

The CMake package defines the following variables and targets:

+----------------------------+------------------------------------------+
| caliper_INCLUDE_DIR        | Caliper include directory (variable)     |
+----------------------------+------------------------------------------+
| caliper                    | The Caliper runtime library (target)     |
+----------------------------+------------------------------------------+
| caliper-serial             | Caliper runtime library without MPI      |
|                            | dependencies (target)                    |
+----------------------------+------------------------------------------+
| caliper-tools-util         | Utilities for caliper tools (target)     |
+----------------------------+------------------------------------------+

In most cases, just link the "caliper" target.
