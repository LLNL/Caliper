set(CMAKE_C_COMPILER   "/usr/local/bin/bgclang" CACHE PATH "")
set(CMAKE_CXX_COMPILER "/usr/local/bin/bgclang++11" CACHE PATH "")

set(MPI_CXX_COMPILER "/usr/local/bin/mpiclang++11" CACHE PATH "")
set(MPI_C_COMPILER   "/usr/local/bin/mpiclang" CACHE PATH "")

# wtf ... not defined in vulcan's cmake platform config
set(CMAKE_AR "/usr/bin/ar" CACHE PATH "")

set(THREADS_PTHREAD_ARG "0" CACHE STRING "")

set(CMAKE_SYSTEM_NAME  "BlueGeneQ-static" CACHE STRING "")

set(BUILD_SHARED_LIBS Off CACHE BOOL "")
set(BUILD_TESTING     Off CACHE BOOL "")

set(WITH_TOOLS     Off CACHE BOOL "")
set(WITH_TEST_APPS Off CACHE BOOL "")

set(WITH_CALLPATH Off CACHE BOOL "")
set(WITH_NVPROF   Off CACHE BOOL "")
set(WITH_CUPTI    Off CACHE BOOL "")
set(WITH_PAPI     Off CACHE BOOL "")
set(WITH_DYNINST  Off CACHE BOOL "")
set(WITH_SAMPLER  Off CACHE BOOL "")
set(WITH_LIBPFM   Off CACHE BOOL "")
set(WITH_MPI      On  CACHE BOOL "")
set(WITH_MPIT     Off CACHE BOOL "")
set(WITH_GOTCHA   Off CACHE BOOL "")

set(CMAKE_IGNORE_PATH "/usr/include;/usr/include/python2.6" CACHE PATH "")
