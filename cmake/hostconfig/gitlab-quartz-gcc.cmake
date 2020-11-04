set(CMAKE_C_COMPILER   "/usr/tce/packages/gcc/gcc-8.1.0/bin/gcc" CACHE PATH "")
set(CMAKE_CXX_COMPILER "/usr/tce/packages/gcc/gcc-8.1.0/bin/g++" CACHE PATH "")
set(CMAKE_Fortran_COMPILER "/usr/tce/packages/gcc/gcc-8.1.0/bin/gfortran" CACHE PATH "")

set(CMAKE_BUILD_TYPE   "RelWithDebInfo" CACHE STRING "")

set(MPI_C_COMPILER     "/usr/tce/packages/mvapich2/mvapich2-2.3-gcc-8.1.0/bin/mpicc" CACHE PATH "")
set(MPI_CXX_COMPILER   "/usr/tce/packages/mvapich2/mvapich2-2.3-gcc-8.1.0/bin/mpicxx" CACHE PATH "")

set(PAPI_PREFIX "/usr/tce/packages/papi/papi-5.5.1" CACHE PATH "")
set(ITT_PREFIX  "/usr/tce/packages/vtune/default" CACHE PATH "")

set(adiak_DIR "/g/g90/boehme3/local/adiak/toss3-0.1.1/lib/cmake/adiak" CACHE PATH "")

set(WITH_FORTRAN  On  CACHE BOOL "")
set(WITH_ADIAK    On  CACHE BOOL "")
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
