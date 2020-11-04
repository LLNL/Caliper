set(CMAKE_C_COMPILER   "/usr/tce/bin/gcc" CACHE PATH "C Compiler (gcc 4.9.3)")
set(CMAKE_CXX_COMPILER "/usr/tce/bin/g++" CACHE PATH "C++ Compiler (g++ 4.9.3)")

set(MPI_C_COMPILER "/usr/tce/packages/mvapich2/mvapich2-2.3-gcc-4.9.3/bin/mpicc" CACHE PATH "")
set(MPI_CXX_COMPILER "/usr/tce/packages/mvapich2/mvapich2-2.3-gcc-4.9.3/bin/mpicxx" CACHE PATH "")

set(PAPI_PREFIX "/usr/tce/packages/papi/papi-5.5.1" CACHE PATH "")
set(ITT_PREFIX  "/usr/tce/packages/vtune/default" CACHE PATH "")
 
set(WITH_LIBUNWIND On  CACHE BOOL "")
set(WITH_NVPROF   Off CACHE BOOL "")
set(WITH_CUPTI    Off CACHE BOOL "")
set(WITH_PAPI     On  CACHE BOOL "")
set(WITH_LIBDW    ON  CACHE BOOL "")
set(WITH_LIBPFM   On  CACHE BOOL "")
set(WITH_SAMPLER  On  CACHE BOOL "")
set(WITH_MPI      On  CACHE BOOL "")
set(WITH_MPIT     Off CACHE BOOL "") # default toss3 mvapich2-2.2 doesn't have any MPI-T stuff
set(WITH_GOTCHA   On  CACHE BOOL "")
set(WITH_VTUNE    On  CACHE BOOL "")

set(WITH_DOCS     Off CACHE BOOL "")
