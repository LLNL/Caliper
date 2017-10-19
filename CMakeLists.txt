#
# Caliper
#

cmake_minimum_required(VERSION 3.1)
project (caliper)

# Version information
set(CALIPER_MAJOR_VERSION 1)
set(CALIPER_MINOR_VERSION 5)
set(CALIPER_PATCH_VERSION 0)
set(CALIPER_VERSION "${CALIPER_MAJOR_VERSION}.${CALIPER_MINOR_VERSION}.${CALIPER_PATCH_VERSION}")

# Add our module directory to the include path.
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

include(GNUInstallDirs)

# Optional Fortran
option(WITH_FORTRAN "Install Fortran interface and build test programs")

# Shared libs option
option(BUILD_SHARED_LIBS "Build shared libraries" TRUE)

# RPATH setup. By default, rpath everything.
option(CMAKE_INSTALL_RPATH_USE_LINK_PATH "Add rpath for all dependencies" TRUE)

option(WITH_TOOLS     "Build Caliper tools" TRUE)
option(WITH_TEST_APPS "Build Caliper test apps" TRUE)
option(WITH_DOCS      "Build Caliper documentation" FALSE)

option(WITH_NVPROF    "Enable NVidia profiler bindings (requires CUDA)" FALSE)
option(WITH_CUPTI     "Enable CUPTI service (CUDA performance analysis)" FALSE)
option(WITH_NETOUT    "Enable netout service (requires curl)" FALSE)
option(WITH_PAPI      "Enable PAPI hardware counter service (requires papi)" TRUE)
option(WITH_LIBPFM    "Enable libpfm (perf_event) sampling" TRUE)
option(WITH_CALLPATH  "Enable callpath service (requires libunwind)" TRUE)
option(WITH_MPI       "Enable MPI" TRUE)
option(WITH_MPIT      "Enable MPI-T" TRUE)
option(WITH_SAMPLER   "Enable sampler (x86 Linux only)" TRUE)
option(WITH_DYNINST   "Enable dyninst (for symbollookup service" FALSE)
option(WITH_GOTCHA    "Enable GOTCHA wrapping" TRUE)

# configure testing explicitly rather than with include(CTest) - avoids some clutter
option(BUILD_TESTING  "Build continuous integration app and unit tests" ON)

if (BUILD_TESTING)
  enable_testing()
endif()

if (BUILD_SHARED_LIBS)
  # the RPATH to be used when installing, but only if it's not a system directory
  list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES ${CMAKE_INSTALL_FULL_LIBDIR} isSystemDir)
  if("${isSystemDir}" STREQUAL "-1")
    set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
  endif("${isSystemDir}" STREQUAL "-1")
else()
  # Try to find static libs first for static builds
  list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 .a)
  # Disable rpaths
  set(CMAKE_SKIP_RPATH TRUE)
endif(BUILD_SHARED_LIBS)

if(WITH_NVPROF)
   find_package(CUDA REQUIRED)

   find_library(NVTX_LIBRARY
     NAME libnvToolsExt.so
     PATHS ${CUDA_TOOLKIT_ROOT_DIR}/lib64 ${CUDA_TOOLKIT_ROOT_DIR}/lib)

   message(STATUS "NVidia tools extension library found in " ${NVTX_LIBRARY})
   set(CALIPER_HAVE_NVPROF ON)
   list(APPEND CALIPER_EXTERNAL_LIBS ${NVTX_LIBRARY})
endif()

if(WITH_CUPTI)
  find_package(CUDA REQUIRED)
  include(FindCUPTI)
  if (CUPTI_FOUND)
    set(CALIPER_HAVE_CUPTI TRUE)
    list(APPEND CALIPER_EXTERNAL_LIBS ${CUPTI_LIBRARY})
  endif()
endif()

if(CALIPER_HAVE_TAU)
    find_library(tau_lib libTAU.so)
    list(APPEND CALIPER_EXTERNAL_LIBS ${tau_lib})
endif()

# Find libunwind
if (WITH_CALLPATH)
  include(FindLibunwind)
  if (LIBUNWIND_FOUND)
    set(CALIPER_HAVE_LIBUNWIND TRUE)
    list(APPEND CALIPER_EXTERNAL_LIBS ${LIBUNWIND_LIBRARY})
  endif()
  include(FindDwarf)
  if (DWARF_FOUND)
    list(APPEND CALIPER_EXTERNAL_LIBS dw)
    add_definitions(-DWITH_DWARF)
  endif()
endif()

if (WITH_NETOUT)
  include(FindLibcurl)
  if (LIBCURL_FOUND)
    set(CALIPER_HAVE_LIBCURL TRUE)
    list(APPEND CALIPER_EXTERNAL_LIBS ${LIBCURL_LIBRARY})
  endif()
endif()

# Find PAPI
if (WITH_PAPI)
  include(FindPAPI)
  if (PAPI_FOUND)
    set(CALIPER_HAVE_PAPI TRUE)
    list(APPEND CALIPER_EXTERNAL_LIBS ${PAPI_LIBRARIES})
  endif()
endif()

# Find libpfm
if (WITH_LIBPFM)
  include(FindLibpfm)
  if(LIBPFM_FOUND)
    message(STATUS "Found perfmon/pfmlib_perf_event.h in " ${LIBPFM_INCLUDE_DIR})
    message(STATUS "Found libpfm.so in " ${LIBPFM_LIBRARY})
    set(CALIPER_HAVE_LIBPFM TRUE)
    list(APPEND CALIPER_EXTERNAL_LIBS ${LIBPFM_LIBRARY})
  else()
    message(STATUS "Could not find libpfm header. \
      use -DLIBPFM_INSTALL=<path to libpfm src directory> \
      e.g. -DLIBPFM_INSTALL=~/papi/src/libpfm4")
  endif()
endif()

# Find dyninst
if (WITH_DYNINST)
  find_package(Dyninst)
  if (Dyninst_FOUND)
    message(STATUS "Found Dyninst in " ${Dyninst_DIR})
    set(CALIPER_HAVE_DYNINST TRUE)
    list(APPEND CALIPER_EXTERNAL_LIBS symtabAPI)
    list(APPEND CALIPER_EXTERNAL_LIBS instructionAPI)
    list(APPEND CALIPER_EXTERNAL_LIBS parseAPI)
  endif()
endif()

if (WITH_GOTCHA)
  set(CALIPER_HAVE_GOTCHA TRUE)
  set(GOTCHA_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/ext/gotcha/gotcha-download/gotcha-src/include)
  list(APPEND CALIPER_EXTERNAL_LIBS gotcha)
endif()

# pthread handling
set(THREADS_PREFER_PTHREAD_FLAG On)
find_package(Threads REQUIRED)

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

if (WITH_MPI)
  find_package(MPI)
  if (MPI_C_FOUND)
    set(CALIPER_HAVE_MPI TRUE)

    if (WITH_MPIT)
      set(CALIPER_HAVE_MPIT TRUE)
    endif()
  endif()
endif()

# Find Python

find_package(PythonInterp REQUIRED)

if (WITH_SAMPLER)
  # Sampler is currently Linux-specific: check for Linux
  if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
    set(CALIPER_HAVE_SAMPLER TRUE)
    # Linux PC sampler needs -lrt
    list(APPEND CALIPER_EXTERNAL_LIBS "-lrt")
    message(STATUS "Linux detected, adding sampler service")
  endif()
endif()

# Create a config header file
configure_file(
  ${PROJECT_SOURCE_DIR}/caliper-config.h.in
  ${PROJECT_BINARY_DIR}/include/caliper/caliper-config.h)

# Include and install header files
include_directories(${PROJECT_BINARY_DIR}/include)
include_directories(include)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h")
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp")

install(FILES ${PROJECT_BINARY_DIR}/include/caliper/caliper-config.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/caliper)

# Create pkg-confic .pc file
set(PKG_CONFIG_INCLUDEDIR "\${prefix}/include")
set(PKG_CONFIG_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
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
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/caliper)
install(EXPORT caliper
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/caliper)

install(FILES ${PROJECT_BINARY_DIR}/caliper.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

add_subdirectory(ext)
add_subdirectory(src)

if (CALIPER_HAVE_MPI)
  add_subdirectory(mpi)
endif()

if (WITH_TEST_APPS)
  add_subdirectory(test)
endif()
if (WITH_DOCS)
  add_subdirectory(doc)
endif()

# Install exports
#install(EXPORT caliper DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
