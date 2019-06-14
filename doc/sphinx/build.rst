Build and Link
================================

Building and installing Caliper requires cmake (version 3.1 or
greater), a current C++11-compatible compiler (GNU 4.8 and greater,
LLVM clang 3.7 and greater are known to work), a Python interpreter,
and the POSIX threads library.

Building Caliper
--------------------------------

To obtain Caliper, clone it from the
`github repository <https://github.com/LLNL/Caliper>`_.

Next, configure and build Caliper:

.. code-block:: sh

     $ cd <path to caliper root directory>
     $ mkdir build && cd build
     $ cmake -DCMAKE_INSTALL_PREFIX=<path to install location> \ 
         -DCMAKE_C_COMPILER=<path to c-compiler> \
         -DCMAKE_CXX_COMPILER=<path to c++-compiler> \
         ..
     $ make 
     $ make install


CMake Flags
................................

You can configure the Caliper build with the following CMake variables:

+---------------------------+----------------------------------------+
| ``CMAKE_INSTALL_PREFIX``  | Directory where Caliper should be      |
|                           | installed.                             |
+---------------------------+----------------------------------------+
| ``CMAKE_C_COMPILER``      | C compiler (with absolute path!)       |
+---------------------------+----------------------------------------+
| ``CMAKE_CXX_COMPILER``    | C++ compiler (with absolute path!)     |
+---------------------------+----------------------------------------+
| ``WITH_FORTRAN``          | Build Fortran test cases and install   |
|                           | Fortran wrapper module.                |
+---------------------------+----------------------------------------+
| ``WITH_TOOLS``            | Build `cali-query`, `cali-graph`, and  |
|                           | `cali-stat` tools.                     |
+---------------------------+----------------------------------------+
| ``BUILD_DOCS``            | Enable documentation builds.           |
+---------------------------+----------------------------------------+
| ``BUILD_TESTING``         | Build unit test infrastructure and     |
|                           | programs.                              |
+---------------------------+----------------------------------------+


Optional modules
................................

Caliper contains a number of modules (*services*) that
provide additional measurement or context data. Some of these services
have additional dependencies.

+--------------+------------------------------+------------------------+
|Service       | Provides                     | Depends on             |
+==============+==============================+========================+
|callpath      | Call path information        | libunwind              |
+--------------+------------------------------+------------------------+
|cupti         | CUDA driver/runtime calls    | CUDA, CUpti            |
+--------------+------------------------------+------------------------+
|libpfm        | Linux perf_event sampling    | Libpfm                 |
+--------------+------------------------------+------------------------+
|mpi           | MPI rank and function calls  | MPI                    |
+--------------+------------------------------+------------------------+
|mpit          | MPI tools interface:         | MPI                    |
|              | MPI-internal counters        |                        |
+--------------+------------------------------+------------------------+
|ompt          | OpenMP thread and status     | OpenMP tools interface |
+--------------+------------------------------+------------------------+
|papi          | PAPI hardware counters       | PAPI library           |
+--------------+------------------------------+------------------------+
|sampler       | Time-based sampling          | x64 Linux              |
+--------------+------------------------------+------------------------+
|symbollookup  | Lookup file/line/function    | Dyninst                |
|              | info from program addresses  |                        |
+--------------+------------------------------+------------------------+
|nvprof        | NVidia NVProf annotation     | CUDA                   |
|              | bindings                     |                        |
+--------------+------------------------------+------------------------+
|vtune         | Intel VTune annotation       | VTune                  |
|              | bindings                     |                        |
+--------------+------------------------------+------------------------+

Building the optional services can be controlled with
``WITH_<service>`` CMake flags. If a flag is set to ``On``, CMake will
try to find the corresponding dependencies. For cases where
dependencies are not found automatically or a specific version is
required, there are typically additional CMake flags to specify the
installation location. For example, to enable PAPI::

    cmake -DWITH_PAPI=On -DPAPI_PREFIX=<path to PAPI installation>

+--------------+-------------------------------------------------------+
|Service       | CMake flags                                           |
+==============+=======================================================+
|callpath      | ``WITH_CALLPATH=On``                                  |
+--------------+-------------------------------------------------------+
|cupti         | ``WITH_CUPTI=On``.                                    |
|              | Set CUpti installation dir in ``CUPTI_PREFIX``.       |
+--------------+-------------------------------------------------------+
|libpfm        | ``WITH_LIBPFM=On``.                                   |
|              | Set libpfm installation dir in ``LIBPFM_INSTALL``.    |
+--------------+-------------------------------------------------------+
|mpi           | ``WITH_MPI=On``.                                      |
|              | Set ``MPI_C_COMPILER`` to path to MPI C compiler.     |
+--------------+-------------------------------------------------------+
|mpit          | ``WITH_MPIT=On``.                                     |
|              | MPI must be enabled.                                  |
+--------------+-------------------------------------------------------+
|papi          | ``WITH_PAPI=On``.                                     |
|              | Set PAPI installation dir in ``PAPI_PREFIX``.         |
+--------------+-------------------------------------------------------+
|sampler       | ``WITH_SAMPLER=On``.                                  |
+--------------+-------------------------------------------------------+
|symbollookup  | ``WITH_DYNINST=On``.                                  |
|              | Set path to ``DyninstConfig.cmake``                   |
|              | in ``Dyninst_DIR``.                                   |
+--------------+-------------------------------------------------------+
|nvprof        | ``WITH_NVPROF=On``.                                   |
|              | Set CUPTI installation dir in ``CUPTI_PREFIX``.       |
+--------------+-------------------------------------------------------+
|vtune         | ``WITH_VTUNE=On``.                                    |
|              | Set Intel ITT API installation dir in ``ITT_PREFIX``. |
+--------------+-------------------------------------------------------+

Linking Caliper programs
--------------------------------

Typically, all that is needed to create a Caliper-enabled program is
to link it with the Caliper runtime library, which resides in
``libcaliper.so``. An example link command for a C++ program built
with g++ could look like this: ::
  
  CALIPER_DIR = /path/to/caliper/installation

  g++ -o target-program $(OBJECTS) -L$(CALIPER_DIR)/lib64 -lcaliper

Static libraries
................................

It is possible to build Caliper as a static library. To do so, set
``BUILD_SHARED_LIBS=Off``::

  cmake -DBUILD_SHARED_LIBS=Off ..

  g++ -o target $(OBJECTS) -L$(CALIPER_DIR)/lib64 -lcaliper

CMake
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

To use Caliper annotations and services, it is sufficient to link the
"caliper" (and "caliper-mpi" for MPI programs) target(s).

Fortran
................................

Caliper provides a Fortran wrapper module in source code form under
``share/fortran/caliper.f90`` in the Caliper installation
directory. This way, we avoid potential incompatibilities between
compilers used to build Caliper and the target program.
We recommend to simply add the Caliper module to the target
program. An example Makefile may look like this: ::

  F90         = gfortran
  
  CALIPER_DIR = /path/to/caliper/installation
  OBJECTS     = main.o
  
  target-program : $(OBJECTS) caliper.o
      $(F90) -o target-program $(OBJECTS) -L$(CALIPER_DIR)/lib64 -lcaliper -lstdc++

  %.o : %.f90 caliper.mod
      $(F90) -c $<

  caliper.mod : caliper.o
      
  caliper.o : $(CALIPER_DIR)/share/fortran/caliper.f90
      $(F90) -std=f2003 -c $<

Note that it is necessary to link in the C++ standard library as
well. With ``gfortran``, add ``-lstdc++``: ::

  gfortran -o target-program *.o -L/path/to/caliper/lib64 -lcaliper -lstdc++
  
With Intel ``ifort``, you can use the ``-cxxlib`` option: ::

  ifort -o target-program *.o -cxxlib -L/path/to/caliper/lib64 -lcaliper

The wrapper module uses Fortran 2003 C bindings. Thus, it requires a
Fortran 2003 compatible compiler to build, but should be usable with
any reasonably "modern" Fortran code. More work may be required to
integrate it with Fortran 77 codes.

