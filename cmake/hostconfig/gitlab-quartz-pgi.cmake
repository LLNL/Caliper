set(CMAKE_C_COMPILER   "/usr/tce/packages/pgi/pgi-20.1/bin/pgcc" CACHE PATH "")
set(CMAKE_CXX_COMPILER "/usr/tce/packages/pgi/pgi-20.1/bin/pgc++" CACHE PATH "")
set(CMAKE_CXX_COMPILER "/usr/tce/packages/pgi/pgi-20.1/bin/pgfortran" CACHE PATH "")

set(CMAKE_CXX_FLAGS    "-std=c++11" CACHE STRING "")

set(MPI_C_COMPILER     "/usr/tce/packages/mvapich2/mvapich2-2.3-pgi-20.1/bin/mpicc" CACHE PATH "")
set(MPI_CXX_COMPILER   "/usr/tce/packages/mvapich2/mvapich2-2.3-pgi-20.1/bin/mpicxx" CACHE PATH "")

set(PAPI_PREFIX "/usr/tce/packages/papi/papi-5.5.1" CACHE PATH "")
set(ITT_PREFIX  "/usr/tce/packages/vtune/default" CACHE PATH "")

set(WITH_FORTRAN  On  CACHE BOOL "")
set(WITH_LIBUNWIND On  CACHE BOOL "")
set(WITH_NVPROF   Off CACHE BOOL "")
set(WITH_CUPTI    Off CACHE BOOL "")
set(WITH_PAPI     On  CACHE BOOL "")
set(WITH_LIBDW    On  CACHE BOOL "")
set(WITH_LIBPFM   On  CACHE BOOL "")
set(WITH_SAMPLER  On  CACHE BOOL "")
set(WITH_MPI      On  CACHE BOOL "")
set(WITH_MPIT     Off CACHE BOOL "") # default toss3 mvapich2-2.2 doesn't have any MPI-T stuff
set(WITH_GOTCHA   On  CACHE BOOL "")
set(WITH_VTUNE    On  CACHE BOOL "")
set(WITH_PCP      On  CACHE BOOL "")

set(WITH_DOCS     Off CACHE BOOL "")
set(BUILD_TESTING On  CACHE BOOL "")
