set(CMAKE_C_COMPILER   "/usr/local/bin/bgclang" CACHE PATH "")
set(CMAKE_CXX_COMPILER "/usr/local/bin/bgclang++11" CACHE PATH "")

# wtf ... not defined in vulcan's cmake platform config
set(CMAKE_AR "/usr/bin/ar" CACHE PATH "")

set(CMAKE_SYSTEM_NAME  "BlueGeneQ-static" CACHE STRING "")

set(BUILD_SHARED_LIBS Off CACHE BOOL "")
set(BUILD_TESTING Off CACHE BOOL "")

set(WITH_TOOLS Off CACHE BOOL "")
set(WITH_TEST_APPS Off CACHE BOOL "")
set(WITH_DYNINST Off CACHE BOOL "")

set(CMAKE_IGNORE_PATH "/usr/include;/usr/include/python2.6" CACHE PATH "")

set(MPI_CXX_COMPILER "/usr/local/bin/mpiclang++11" CACHE PATH "")
set(MPI_C_COMPILER "/usr/local/bin/mpiclang" CACHE PATH "")
