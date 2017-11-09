#
# Caliper
#

cmake_minimum_required(VERSION 3.1)
project (caliper)

# Version information
set(CALIPER_MAJOR_VERSION 1)
set(CALIPER_MINOR_VERSION 6)
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
option(WITH_LIBDW     "Enable libdw support (for module detection in callpath service)" FALSE)
option(WITH_CALLPATH  "Enable callpath service (requires libunwind)" TRUE)
option(WITH_MPI       "Enable MPI" TRUE)
option(WITH_MPIT      "Enable MPI-T" TRUE)
option(WITH_OMPT      "Enable OMPT" FALSE)
option(WITH_SAMPLER   "Enable Linux sampler (x86 Linux only)" TRUE)
option(WITH_DYNINST   "Enable dyninst (for symbollookup service" FALSE)
option(WITH_GOTCHA    "Enable GOTCHA wrapping" TRUE)
option(WITH_SOS       "Enable SOSFlow data management" FALSE)
option(WITH_VTUNE     "Enable Intel VTune annotation bindings" FALSE)

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

if(WITH_VTUNE)
  include(FindITTAPI)
  if (ITT_FOUND)
    set(CALIPER_HAVE_VTUNE TRUE)
    set(CALIPER_VTune_CMAKE_MSG "Yes, using ${ITT_LIBRARY}")
    # libittnotify.a needs libdl
    list(APPEND CALIPER_EXTERNAL_LIBS ${ITT_LIBRARY} "-ldl")
  else()
    message(WARNING "VTune bindings requested but Intel ITT API was not found!\n"
  "Set ITT_PREFIX to ittnotify installation path and re-run cmake.")
  endif()
endif()

if(WITH_NVPROF)
   find_package(CUDA REQUIRED)

   find_library(NVTX_LIBRARY
     NAME libnvToolsExt.so
     PATHS ${CUDA_TOOLKIT_ROOT_DIR}/lib64 ${CUDA_TOOLKIT_ROOT_DIR}/lib)

   message(STATUS "NVidia tools extension library found in " ${NVTX_LIBRARY})
   set(CALIPER_HAVE_NVPROF ON)
   set(CALIPER_NVProf_CMAKE_MSG "Yes, using ${NVTX_LIBRARY}")
   list(APPEND CALIPER_EXTERNAL_LIBS ${NVTX_LIBRARY})
endif()

if(WITH_CUPTI)
  find_package(CUDA REQUIRED)
  include(FindCUPTI)
  if (CUPTI_FOUND)
    set(CALIPER_HAVE_CUPTI TRUE)
    set(CALIPER_CUpti_CMAKE_MSG "Yes, using ${CUPTI_LIBRARY}")
    list(APPEND CALIPER_EXTERNAL_LIBS ${CUPTI_LIBRARY})
  endif()
endif()

if(CALIPER_HAVE_TAU)
    find_library(tau_lib libTAU.so)
    list(APPEND CALIPER_EXTERNAL_LIBS ${tau_lib})
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
    set(CALIPER_PAPI_CMAKE_MSG "Yes, using ${PAPI_LIBRARIES}")
    list(APPEND CALIPER_EXTERNAL_LIBS ${PAPI_LIBRARIES})
  else()
    message(WARNING "PAPI support was requested but PAPI was not found!\n"
  "Set PAPI_PREFIX to the PAPI installation path and re-run cmake.")
  endif()
endif()

# Find libpfm
if (WITH_LIBPFM)
  include(FindLibpfm)
  if(LIBPFM_FOUND)
    message(STATUS "Found perfmon/pfmlib_perf_event.h in " ${LIBPFM_INCLUDE_DIR})
    message(STATUS "Found libpfm.so in " ${LIBPFM_LIBRARY})
    set(CALIPER_HAVE_LIBPFM TRUE)
    set(CALIPER_Libpfm_CMAKE_MSG "Yes, using ${LIBPFM_LIBRARY}")
    list(APPEND CALIPER_EXTERNAL_LIBS ${LIBPFM_LIBRARY})
  else()
    message(WARNING "Libpfm support was requested but libpfm.so was not found!\n"
      "Set -DLIBPFM_INSTALL=<path to libpfm src directory (e.g. -DLIBPFM_INSTALL=~/papi/src/libpfm4)"
      "and re-run cmake")
  endif()
endif()

# Find dyninst
if (WITH_DYNINST)
  find_package(Dyninst)
  if (Dyninst_FOUND)
    message(STATUS "Found Dyninst in " ${Dyninst_DIR})
    set(CALIPER_HAVE_DYNINST TRUE)
    set(CALIPER_Dyninst_CMAKE_MSG "Yes, using installation in ${Dyninst_DIR}")
    list(APPEND CALIPER_EXTERNAL_LIBS symtabAPI)
    list(APPEND CALIPER_EXTERNAL_LIBS instructionAPI)
    list(APPEND CALIPER_EXTERNAL_LIBS parseAPI)
  endif()
  # CMake already provides useful error message if DyninstConfig.cmake isn't found
endif()

# Find libunwind
if (WITH_CALLPATH)
  include(FindLibunwind)
  if (LIBUNWIND_FOUND)
    set(CALIPER_HAVE_LIBUNWIND TRUE)
    set(CALIPER_Libunwind_CMAKE_MSG "Yes, using ${LIBUNWIND_LIBRARY}")
    list(APPEND CALIPER_EXTERNAL_LIBS ${LIBUNWIND_LIBRARY})
  else()
    message(WARNING "Callpath support was requested but libunwind was not found!")
  endif()
  if (WITH_LIBDW)
    # For some reason Dyninst crashes when libdw is linked.
    # Detect this here and disable libdw.
    if (Dyninst_FOUND)
      message(WARNING "Libdw option is incompatible with dyninst -- disabling libdw.")
    else()
      include(FindLibDw)
      if (LIBDW_FOUND)
        message(STATUS "Found libdw in " ${LIBDW_LIBRARY})
        list(APPEND CALIPER_EXTERNAL_LIBS ${LIBDW_LIBRARY})
        set(CALIPER_HAVE_LIBDW TRUE)
      endif()
    endif()
  endif()
endif()

if (WITH_GOTCHA)
  set(CALIPER_HAVE_GOTCHA TRUE)
  set(CALIPER_GOTCHA_CMAKE_MSG "Yes")
  set(GOTCHA_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/ext/gotcha/gotcha-download/gotcha-src/include)
  list(APPEND CALIPER_EXTERNAL_LIBS gotcha)
endif()

if (WITH_SOS)
  include(FindSOSFlow)
  if (SOSFlow_FOUND)
    message(STATUS "Found sosflow in " ${SOSFlow_LIBRARY})
    list(APPEND CALIPER_EXTERNAL_LIBS ${SOSFlow_LIBRARY})
    set(CALIPER_HAVE_SOS TRUE)
    set(CALIPER_SOSFlow_CMAKE_MSG "Yes, using ${SOSFlow_LIBRARY}")
  endif()
endif()

# pthread handling
set(THREADS_PREFER_PTHREAD_FLAG On)
find_package(Threads REQUIRED)

if (WITH_OMPT)
  # Find OMPT header
  find_path(OMPT_INCLUDE_DIR ompt.h
    PATH_SUFFIXES include
    HINTS $ENV{OMPT_DIR} ${OMPT_DIR})

  if (OMPT_INCLUDE_DIR)
    set(OMPT_FOUND TRUE)
    set(CALIPER_HAVE_OMPT TRUE)
    set(CALIPER_OMPT_CMAKE_MSG "Yes, using ${OMPT_INCLUDE_DIR}")
  else()
    message(WARNING "OpenMP tools interface (OMPT) support requested but ompt.h not found!\n"
      "Set OMPT_DIR to OpenMP tools interface installation directory and re-run cmake.")
  endif()
endif()

# Find MPI
if (WITH_MPI)
  find_package(MPI)
  if (MPI_C_FOUND)
    set(CALIPER_HAVE_MPI TRUE)
    set(CALIPER_MPI_CMAKE_MSG "Yes, using ${MPI_C_LIBRARIES}")

    if (WITH_MPIT)
      set(CALIPER_HAVE_MPIT TRUE)
      set(CALIPER_MPIT_CMAKE_MSG "Yes")
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
  else()
    message(WARNING "Sampler is not supported on ${CMAKE_SYSTEM_NAME}")
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

#
# Print config summary
#

message(STATUS "Caliper configuration summary:")

message(STATUS "Caliper version           : ${CALIPER_VERSION}")
message(STATUS "Build type                : ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler                  : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} (${CMAKE_CXX_COMPILER})")
message(STATUS "Install dir               : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Build shared libs         : ${BUILD_SHARED_LIBS}")

set(CALIPER_MODULES
  GOTCHA
  PAPI
  Libpfm
  Libunwind
  Dyninst
  SOSFlow
  MPI
  MPIT
  OMPT
  NVProf
  CUpti
  VTune)

foreach(_caliper_module ${CALIPER_MODULES})
  string(LENGTH "${_caliper_module}" _strlen)
  string(SUBSTRING "                " ${_strlen} "-1" _padding)
  set(_prefix "${_caliper_module} support ${_padding}")

  if (DEFINED CALIPER_${_caliper_module}_CMAKE_MSG)
    message(STATUS "${_prefix} : ${CALIPER_${_caliper_module}_CMAKE_MSG}")
  else()
    message(STATUS "${_prefix} : No")
  endif()
endforeach()

# Install exports
#install(EXPORT caliper DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
