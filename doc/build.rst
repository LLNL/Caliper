Build and install
================================

Building and installing Caliper requires cmake and a current C++11-compatible
Compiler. Unpack the source distribution and proceed as follows:

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

Caliper contains a number of modules (*measurement services*) that
provide additional measurement or context data. Most of these services
have additional dependencies. Therefore, these services are optional
and only built when their dependencies are found.

+------------+------------------------------+------------------------+
|Service     | Provides                     | Depends on             |
+============+==============================+========================+
|callpath    | Call path information        | libunwind              |
+------------+------------------------------+------------------------+
|mitos       | Linux perf-event data        | Mitos library          |
+------------+------------------------------+------------------------+
|mpi         | MPI rank and function calls  | MPI                    |
+------------+------------------------------+------------------------+
|ompt        | OpenMP thread and status     | OpenMP tools interface |
+------------+------------------------------+------------------------+
|papi        | PAPI hardware counters       | PAPI library           |
+------------+------------------------------+------------------------+


Common CMake Flags
--------------------------------

You can configure the Caliper build with the following CMake variables:

+---------------------------+----------------------------------------+
| `CMAKE_INSTALL_PREFIX`    | Directory where Caliper should be      |
|                           | installed                              |
+---------------------------+----------------------------------------+
| `CMAKE_C_COMPILER`        | C compiler (with absolute path!)       |
+---------------------------+----------------------------------------+
| `CMAKE_CXX_COMPILER`      | C++ compiler (with absolute path!)     |
+---------------------------+----------------------------------------+
| `MPI_C_COMPILER`          | MPI C compiler                         |
+---------------------------+----------------------------------------+
| `MPI_CXX_COMPILER`        | MPI C++ compiler                       |
+---------------------------+----------------------------------------+
| `OMPT_DIR`                | Path to the OpenMP tools interface     |
|                           | header (ompt.h                         |
+---------------------------+----------------------------------------+
