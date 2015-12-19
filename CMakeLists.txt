#
# Caliper
#

project (caliper)
cmake_minimum_required(VERSION 3.0)

# Version information
set(CALIPER_MAJOR_VERSION 1)
set(CALIPER_MINOR_VERSION 1)
set(CALIPER_PATCH_VERSION 0)
set(CALIPER_VERSION "${CALIPER_MAJOR_VERSION}.${CALIPER_MINOR_VERSION}.${CALIPER_PATCH_VERSION}")

# Add our module directory to the include path.
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
include(FindLibunwind)
include(FindPAPI)

# Shared libs option
option(BUILD_SHARED_LIBS "Build shared libraries" TRUE)

# RPATH setup. By default, rpath everything.
option(CMAKE_INSTALL_RPATH_USE_LINK_PATH "Add rpath for all dependencies" TRUE)

# the RPATH to be used when installing, but only if it's not a system directory
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
if("${isSystemDir}" STREQUAL "-1")
   set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif("${isSystemDir}" STREQUAL "-1")

# Find libunwind
if (LIBUNWIND_FOUND)
  set(CALIPER_HAVE_LIBUNWIND TRUE)
  list(APPEND CALIPER_EXTERNAL_LIBS ${LIBUNWIND_LIBRARY})
endif()

# Find PAPI
if (PAPI_FOUND)
  set(CALIPER_HAVE_PAPI TRUE)
  list(APPEND CALIPER_EXTERNAL_LIBS ${PAPI_LIBRARIES})
endif()

find_package(Threads)

# Find OMPT header
find_path(OMPT_INCLUDE_DIR ompt.h
  PATH_SUFFIXES include
  HINTS $ENV{OMPT_DIR} ${OMPT_DIR})

if (OMPT_INCLUDE_DIR)
  set(OMPT_FOUND TRUE)
  set(CALIPER_HAVE_OMPT TRUE)
  message(STATUS "OpenMP tools interface header ompt.h found in " ${OMPT_INCLUDE_DIR})
else()
  message(STATUS "OpenMP tools interface header ompt.h not found")
endif()

# Find MPI

find_package(MPI QUIET)

if (MPI_C_FOUND)
  set(CALIPER_HAVE_MPI TRUE)
endif()

# Create a config header file
configure_file(
  ${PROJECT_SOURCE_DIR}/caliper-config.h.in
  ${PROJECT_BINARY_DIR}/caliper-config.h)

# Make caliper findable
configure_file(
  ${PROJECT_SOURCE_DIR}/caliper-config.cmake.in
  ${PROJECT_BINARY_DIR}/caliper-config.cmake
  @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/caliper-config.cmake DESTINATION share/cmake/caliper)
install(EXPORT caliper                                   DESTINATION share/cmake/caliper)


install(FILES ${PROJECT_BINARY_DIR}/caliper-config.h DESTINATION include)

add_subdirectory(doc)
add_subdirectory(src)
add_subdirectory(test)

# Install exports
#install(EXPORT caliper DESTINATION lib/cmake)
