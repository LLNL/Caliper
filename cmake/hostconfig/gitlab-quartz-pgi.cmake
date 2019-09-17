set(CMAKE_C_COMPILER   "/usr/tce/packages/pgi/pgi-19.7/bin/pgcc" CACHE PATH "")
set(CMAKE_CXX_COMPILER "/usr/tce/packages/pgi/pgi-19.7/bin/pgc++" CACHE PATH "")

set(CMAKE_CXX_FLAGS    "-std=c++11" CACHE STRING "")

set(MPI_C_COMPILER     "/usr/tce/packages/mvapich2/mvapich2-2.3-pgi-19.7/bin/mpicc" CACHE PATH "")
set(MPI_CXX_COMPILER   "/usr/tce/packages/mvapich2/mvapich2-2.3-pgi-19.7/bin/mpicxx" CACHE PATH "")

set(PAPI_PREFIX "/usr/tce/packages/papi/papi-5.5.1" CACHE PATH "")
set(ITT_PREFIX  "/usr/tce/packages/vtune/default" CACHE PATH "")

# DBO 2019-02-28: dyninst-10/boost installation on toss3 is broken, leading to
# undefined references or non-existing include files. Turn it off for now.
# Build with spack if dyninst is needed.
#set(CMAKE_PREFIX_PATH "/usr/tce/packages/dyninst/dyninst-10.0.0/lib/cmake" CACHE PATH "")
 
set(WITH_CALLPATH On  CACHE BOOL "")
set(WITH_NVPROF   Off CACHE BOOL "")
set(WITH_CUPTI    Off CACHE BOOL "")
set(WITH_PAPI     On  CACHE BOOL "")
set(WITH_LIBPFM   On  CACHE BOOL "")
set(WITH_DYNINST  Off CACHE BOOL "") # turn off for now
set(WITH_SAMPLER  On  CACHE BOOL "")
set(WITH_MPI      On  CACHE BOOL "")
set(WITH_MPIT     Off CACHE BOOL "") # default toss3 mvapich2-2.2 doesn't have any MPI-T stuff
set(WITH_GOTCHA   On  CACHE BOOL "")
set(WITH_VTUNE    On  CACHE BOOL "")

set(WITH_DOCS     Off CACHE BOOL "")
set(BUILD_TESTING On  CACHE BOOL "")
