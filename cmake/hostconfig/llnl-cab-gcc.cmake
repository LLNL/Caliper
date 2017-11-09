set(CMAKE_C_COMPILER   "/usr/apps/gnu/4.9.3/bin/gcc" CACHE PATH "C Compiler (gcc 4.9.3)")
set(CMAKE_CXX_COMPILER "/usr/apps/gnu/4.9.3/bin/g++" CACHE PATH "C++ Compiler (g++ 4.9.3)")

set(MPI_C_COMPILER   "/usr/local/tools/mvapich2-gnu-2.2/bin/mpicc"  CACHE PATH "")
set(MPI_CXX_COMPILER "/usr/local/tools/mvapich2-gnu-2.2/bin/mpicxx" CACHE PATH "")

set(PAPI_PREFIX "/usr/local/tools/papi-5.4.0" CACHE PATH "")
set(ITT_PREFIX  "/usr/local/tools/vtune" CACHE PATH "")

set(WITH_CALLPATH On  CACHE BOOL "")
set(WITH_NVPROF   Off CACHE BOOL "")
set(WITH_CUPTI    Off CACHE BOOL "")
set(WITH_PAPI     On  CACHE BOOL "")
set(WITH_DYNINST  Off CACHE BOOL "")
set(WITH_SAMPLER  On  CACHE BOOL "")
set(WITH_LIBPFM   Off CACHE BOOL "")
set(WITH_MPI      On  CACHE BOOL "")
set(WITH_MPIT     Off CACHE BOOL "")
set(WITH_GOTCHA   On  CACHE BOOL "")
set(WITH_VTUNE    On  CACHE BOOL "")

set(WITH_DOCS     Off CACHE BOOL "")
