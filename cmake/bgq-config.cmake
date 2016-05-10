set(CMAKE_C_COMPILER   "/usr/local/bin/bgclang" CACHE PATH "")
set(CMAKE_CXX_COMPILER "/usr/local/bin/bgclang++11" CACHE PATH "")

set(BUILD_SHARED_LIBS Off CACHE BOOL "")

set(WITH_TOOLS Off CACHE BOOL "")
set(WITH_TESTS Off CACHE BOOL "")

set(CMAKE_IGNORE_PATH "/usr/include;/usr/include/python2.6" CACHE PATH "")

set(MPI_CXX_COMPILER "/usr/local/bin/mpiclang++11" CACHE PATH "")
