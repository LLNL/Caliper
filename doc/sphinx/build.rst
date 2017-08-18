Build and install
================================

Building and installing Caliper requires cmake (version 3.0 or
greater), a current C++11-compatible compiler (GNU 4.8 and greater,
LLVM clang 3.7 and greater are known to work), a Python interpreter,
and the POSIX threads library.

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

Optional modules
--------------------------------

Caliper contains a number of modules (*services*) that
provide additional measurement or context data. Some of these services
have additional dependencies:

+------------+------------------------------+------------------------+
|Service     | Provides                     | Depends on             |
+============+==============================+========================+
|callpath    | Call path information        | libunwind              |
+------------+------------------------------+------------------------+
|libpfm      | Linux perf_event sampling    | Libpfm / Linux OS      |
+------------+------------------------------+------------------------+
|mpi         | MPI rank and function calls  | MPI                    |
+------------+------------------------------+------------------------+
|ompt        | OpenMP thread and status     | OpenMP tools interface |
+------------+------------------------------+------------------------+
|papi        | PAPI hardware counters       | PAPI library           |
+------------+------------------------------+------------------------+
|sampler     | Timer-based sampling         | Linux OS               |
+------------+------------------------------+------------------------+

These services are optional and will only be built when their
dependencies are found.

Common CMake Flags
--------------------------------

You can configure the Caliper build with the following CMake variables:

+---------------------------+----------------------------------------+
| ``CMAKE_INSTALL_PREFIX``  | Directory where Caliper should be      |
|                           | installed                              |
+---------------------------+----------------------------------------+
| ``CMAKE_C_COMPILER``      | C compiler (with absolute path!)       |
+---------------------------+----------------------------------------+
| ``CMAKE_CXX_COMPILER``    | C++ compiler (with absolute path!)     |
+---------------------------+----------------------------------------+
| ``MPI_C_COMPILER``,       | MPI C and C++ compilers for optional   |
| ``MPI_CXX_COMPILER``      | MPI wrapper module                     |
+---------------------------+----------------------------------------+
| ``OMPT_DIR``              | Path to the OpenMP tools interface     |
|                           | header (ompt.h                         |
+---------------------------+----------------------------------------+
| ``WITH_FORTRAN``          | Build Fortran test cases and install   |
|                           | Fortran wrapper module                 |
+---------------------------+----------------------------------------+
| ``WITH_TESTS``            | Build small example test programs      |
+---------------------------+----------------------------------------+
| ``WITH_TOOLS``            | Build `cali-query`, `cali-graph`, and  |
|                           | `cali-stat` tools.                     |
+---------------------------+----------------------------------------+
| ``WITH_DOCS``             | Build Sphinx documentation.            |
+---------------------------+----------------------------------------+

Building on BG/Q
--------------------------------

When building on a BlueGene/Q system, the libraries must be cross-compiled to
work correctly on the compute nodes. Use the provided toolchain file to build
with clang, like so:

.. code-block:: sh

     $ cd <path to caliper root directory>
     $ mkdir build && cd build
     $ cmake -DCMAKE_INSTALL_PREFIX=<path to install location> \ 
         -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/bgq-dynamic.toolchain \
         -DCMAKE_CXX_FLAGS=-stdlib=libc++ \
         ..
     $ make 
     $ make install

When processing the created cali files, make sure to use a version of
`cali-query` complied for the frontend node. 
