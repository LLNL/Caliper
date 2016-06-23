#
# Caliper
#

project (caliper)
cmake_minimum_required(VERSION 3.0)

# Version information
set(CALIPER_MAJOR_VERSION 1)
set(CALIPER_MINOR_VERSION 5)
set(CALIPER_PATCH_VERSION dev)
set(CALIPER_VERSION "${CALIPER_MAJOR_VERSION}.${CALIPER_MINOR_VERSION}.${CALIPER_PATCH_VERSION}")

# Add our module directory to the include path.
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
include(FindLibunwind)
include(FindPAPI)
include(FindLibcurl)
# Optional Fortran
option(WITH_FORTRAN "Install Fortran interface and build test programs")

# Shared libs option
option(BUILD_SHARED_LIBS "Build shared libraries" TRUE)

# RPATH setup. By default, rpath everything.
option(CMAKE_INSTALL_RPATH_USE_LINK_PATH "Add rpath for all dependencies" TRUE)

option(WITH_TOOLS "Build Caliper tools" TRUE)
option(WITH_TESTS "Build Caliper test apps" TRUE)

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

if (LIBCURL_FOUND)
  set(CALIPER_HAVE_LIBCURL TRUE)
  list(APPEND CALIPER_EXTERNAL_LIBS ${LIBCURL_LIBRARY})
endif()

# Find PAPI
if (PAPI_FOUND)
  set(CALIPER_HAVE_PAPI TRUE)
  list(APPEND CALIPER_EXTERNAL_LIBS ${PAPI_LIBRARIES})
endif()

# Find Mitos

find_package(Mitos QUIET)
if(Mitos_FOUND)
  find_package(Dyninst REQUIRED SymtabAPI InstructionAPI ParseAPI)
  set(CALIPER_HAVE_MITOS TRUE)
  list(APPEND CALIPER_EXTERNAL_LIBS mitos hwloc symtabAPI instructionAPI parseAPI)
  message(STATUS "Mitos found")
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

# Find Python

find_package(PythonLibs)

# Sampler is currently Linux-specific: check for Linux
if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
  set(CALIPER_HAVE_SAMPLER TRUE)
  # Linux PC sampler needs -lrt
  list(APPEND CALIPER_EXTERNAL_LIBS "-lrt")
  message(STATUS "Linux detected, adding sampler service")
endif()

# Create a config header file
configure_file(
  ${PROJECT_SOURCE_DIR}/caliper-config.h.in
  ${PROJECT_BINARY_DIR}/caliper-config.h)

# Create pkg-confic .pc file
set(PKG_CONFIG_INCLUDEDIR "\${prefix}/include")
set(PKG_CONFIG_LIBDIR "\${prefix}/lib")
set(PKG_CONFIG_LIBS "-L\${libdir} -lcaliper")
set(PKG_CONFIG_CFLAGS "-I\${includedir}")

configure_file(
  ${PROJECT_SOURCE_DIR}/caliper.pc.in
  ${PROJECT_BINARY_DIR}/caliper.pc)

# Make caliper findable for cmake
configure_file(
  ${PROJECT_SOURCE_DIR}/caliper-config.cmake.in
  ${PROJECT_BINARY_DIR}/caliper-config.cmake
  @ONLY)

install(FILES ${PROJECT_BINARY_DIR}/caliper-config.cmake 
  DESTINATION share/cmake/caliper)
install(EXPORT caliper
  DESTINATION share/cmake/caliper)

install(FILES ${PROJECT_BINARY_DIR}/caliper-config.h 
  DESTINATION include/caliper)

install(FILES ${PROJECT_BINARY_DIR}/caliper.pc 
  DESTINATION share/pkgconfig)

add_subdirectory(doc)
add_subdirectory(src)
if (WITH_TESTS)
  add_subdirectory(test)
endif()

# Install exports
#install(EXPORT caliper DESTINATION lib/cmake)
