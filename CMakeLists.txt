#
# Caliper
#

cmake_minimum_required(VERSION 3.12)
project (caliper C CXX)

# ensure visibility gets honored for all targets
cmake_policy(SET CMP0063 NEW)
# Use <PackageName>_ROOT variables in find_package()
cmake_policy(SET CMP0074 NEW)
# ensure normal variables from parent projects are honored by option()
if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()
# Find Python by location
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.15)
  cmake_policy(SET CMP0094 NEW)
endif()

# set the default visibility settings
set(CMAKE_VISIBILITY_INLINES_HIDDEN OFF)
set(CMAKE_C_VISIBILITY_PRESET "default")
set(CMAKE_CXX_VISIBILITY_PRESET "default")

# Version information
set(CALIPER_MAJOR_VERSION 2)
set(CALIPER_MINOR_VERSION 13)
set(CALIPER_PATCH_VERSION 0)
set(CALIPER_VERSION "${CALIPER_MAJOR_VERSION}.${CALIPER_MINOR_VERSION}.${CALIPER_PATCH_VERSION}-dev")

# Add our module directory to the include path.
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

include(GNUInstallDirs)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(CALIPER_HAVE_LINUX TRUE)
endif()

# generic options starting with WITH_ can conflict when caliper is submodule
option(CALIPER_OPTION_PREFIX "Option names are prefixed with 'CALIPER_'" OFF)
mark_as_advanced(CALIPER_OPTION_PREFIX)

macro(ADD_CALIPER_OPTION NAME)
  if(CALIPER_OPTION_PREFIX)
    option(CALIPER_${NAME} ${ARGN})
      # set the option locally for this project
      set(${NAME} ${CALIPER_${NAME}})
  else()
     option(${NAME} ${ARGN})
  endif()
endmacro()

# Shared libs option
option(BUILD_SHARED_LIBS "Build shared libraries" TRUE)
# RPATH setup. By default, rpath everything.
option(CMAKE_INSTALL_RPATH_USE_LINK_PATH "Add rpath for all dependencies" TRUE)

# Optional Fortran
add_caliper_option(WITH_FORTRAN           "Install Fortran interface" FALSE)
add_caliper_option(WITH_PYTHON_BINDINGS   "Install Python bindings" FALSE)
add_caliper_option(WITH_TOOLS             "Build Caliper tools" TRUE)

add_caliper_option(WITH_NVTX      "Enable NVidia nvtx bindings for NVprof and NSight (requires CUDA)" FALSE)
add_caliper_option(WITH_CUPTI     "Enable CUPTI service (CUDA performance analysis)" FALSE)
add_caliper_option(WITH_PAPI      "Enable PAPI hardware counter service (requires papi)" FALSE)
add_caliper_option(WITH_LIBPFM    "Enable libpfm (perf_event) sampling" FALSE)
add_caliper_option(WITH_LIBDW     "Enable libdw support (for symbollookup service)" FALSE)
add_caliper_option(WITH_LIBUNWIND "Enable libunwind support (for callpath service)" FALSE)
add_caliper_option(WITH_MPI       "Enable MPI" FALSE)
# add_caliper_option(WITH_MPIT      "Enable MPI-T" FALSE)
add_caliper_option(WITH_OMPT      "Enable OMPT" FALSE)
add_caliper_option(WITH_SAMPLER   "Enable Linux sampler (x86 and PPC Linux only)" FALSE)
add_caliper_option(WITH_GOTCHA    "Enable GOTCHA wrapping" ${CALIPER_HAVE_LINUX})
add_caliper_option(WITH_ROCTX     "Enable AMD RocTX support" FALSE)
add_caliper_option(WITH_ROCTRACER "Enable AMD RocTracer support" FALSE)
add_caliper_option(WITH_ROCM      "Enable AMD RocTracer and RocTX support" FALSE)
add_caliper_option(WITH_ROCPROFILER "Enable AMD rocprofiler-sdk support" FALSE)
add_caliper_option(WITH_TAU       "Enable TAU service (TAU Performance System)" FALSE)
add_caliper_option(WITH_VTUNE     "Enable Intel(R) VTune(tm) annotation bindings" FALSE)
add_caliper_option(WITH_ADIAK     "Enable adiak support" FALSE)
add_caliper_option(WITH_KOKKOS    "Enable Kokkos profiling support" TRUE)
add_caliper_option(WITH_PCP       "Enable performance co-pilot support" FALSE)
add_caliper_option(WITH_VARIORUM  "Enable Variorum support" FALSE)
add_caliper_option(WITH_UMPIRE    "Enable Umpire statistics support" FALSE)
add_caliper_option(WITH_CRAYPAT   "Enable CrayPAT region forwarding support" FALSE)
add_caliper_option(WITH_LDMS      "Enable LDMS forwarder" FALSE)
add_caliper_option(WITH_PAPI_RDPMC "Declare that PAPI is built to use rdpmc for reading counters. Does nothing if PAPI support is not enabled." TRUE)

if (WITH_PAPI_RDPMC)
  set(CALIPER_WITH_PAPI_RDPMC TRUE)
endif ()

set(WITH_ARCH "" CACHE STRING "Enable features specific to the provided archspec CPU architecture name")
if (NOT WITH_ARCH STREQUAL "")
  string(TOLOWER ${WITH_ARCH} LOWER_WITH_ARCH)
  set(CALIPER_HAVE_ARCH "${LOWER_WITH_ARCH}")
endif ()

add_caliper_option(USE_EXTERNAL_GOTCHA "Use pre-installed gotcha instead of building our own" FALSE)

add_caliper_option(ENABLE_HISTOGRAMS "Enable histogram aggregation (experimental)" FALSE)

# configure testing explicitly rather than with include(CTest) - avoids some clutter
add_caliper_option(BUILD_TESTING  "Build continuous integration app and unit tests" FALSE)
add_caliper_option(BUILD_DOCS     "Build Caliper documentation" FALSE)

add_caliper_option(RUN_MPI_TESTS  "Run MPI tests (only applicable with BUILD_TESTING=On)" TRUE)

# allow disabling cmake/pkg-config files and headers for when caliper is a subproject
add_caliper_option(INSTALL_CONFIG  "Install cmake and pkg-config files" TRUE)
add_caliper_option(INSTALL_HEADERS "Install caliper headers" TRUE)

## Find Shroud
## Doesn't work for me :-/ Generating wrapper manually.
# if (EXISTS ${SHROUD_EXECUTABLE})
#   execute_process(COMMAND ${SHROUD_EXECUTABLE}
#     --cmake ${CMAKE_CURRENT_BINARY_DIR}/SetupShroud.cmake
#     ERROR_VARIABLE SHROUD_cmake_error
#     OUTPUT_STRIP_TRAILING_WHITESPACE)
#   if (${SHROUD_cmake_error})
#     message(FATAL_ERROR "Error from Shroud: ${SHROUD_cmake_error}")
#   endif ()
#   include(${CMAKE_CURRENT_BINARY_DIR}/SetupShroud.cmake)
# endif ()
# if (${SHROUD_FOUND})
#   set(CALIPER_Shroud_CMAKE_MSG "Yes, using ${SHROUD_EXECUTABLE}")
#   add_subdirectory(src/interface)
# else()
#   set(CALIPER_Shroud_CMAKE_MSG "No")
# endif()

if (WITH_FORTRAN)
  enable_language(Fortran)
  set(CMAKE_Fortran_MODULE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fortran")
  set(CALIPER_Fortran_CMAKE_MSG "Yes, using ${CMAKE_Fortran_COMPILER_ID} ${CMAKE_Fortran_COMPILER_VERSION}")
  set(CALIPER_HAVE_FORTRAN TRUE)
endif()

if (BUILD_TESTING)
  enable_testing()
endif()

if (BUILD_SHARED_LIBS)
  # the RPATH to be used when installing, but only if it's not a system directory
  list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES ${CMAKE_INSTALL_FULL_LIBDIR} isSystemDir)
  if("${isSystemDir}" STREQUAL "-1")
    list(APPEND CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
  endif("${isSystemDir}" STREQUAL "-1")
else()
  # Try to find static libs first for static builds
  # list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 .a)
  # Disable rpaths
  set(CMAKE_SKIP_RPATH TRUE)
endif(BUILD_SHARED_LIBS)

if (${CALIPER_HAVE_LINUX})
  set(CALIPER_HAVE_CPUINFO TRUE)
  set(CALIPER_HAVE_MEMUSAGE TRUE)
  set(CALIPER_cpuinfo_CMAKE_MSG "Yes")
  set(CALIPER_memusage_CMAKE_MSG "Yes")
endif()

if (ENABLE_HISTOGRAMS)
  set(CALIPER_ENABLE_HISTOGRAMS TRUE)
  set(CALIPER_histogram_CMAKE_MSG "Yes")
endif()

if(WITH_VTUNE)
  include(FindITTAPI)
  if (ITT_FOUND)
    set(CALIPER_HAVE_VTUNE TRUE)
    set(CALIPER_VTune_CMAKE_MSG "Yes, using ${ITT_LIBRARY}")
    # libittnotify.a needs libdl
    list(APPEND CALIPER_EXTERNAL_LIBS ${ITT_LIBRARY} ${CMAKE_DL_LIBS})
  else()
    message(WARNING "VTune bindings requested but Intel ITT API was not found!\n"
  "Set ITT_PREFIX to ittnotify installation path and re-run cmake.")
  endif()
endif()

if (WITH_NVTX)
  if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.17)
    find_package(CUDAToolkit REQUIRED)

    if (CUDAToolkit_FOUND)
      set(CALIPER_HAVE_NVTX ON)
      set(CALIPER_Nvtx_CMAKE_MSG "Yes, using ${CUDAToolkit_TARGET_DIR}")
      set(CALI_NVTX_INCLUDE_DIRS ${CUDAToolkit_INCLUDE_DIRS})
    endif()
  else()
    find_package(CUDA REQUIRED)

    find_path(CALI_NVTX_INCLUDE_DIRS
      NAME nvtx3/nvToolsExt.h
      PATHS ${CUDA_TOOLKIT_ROOT_DIR}/include)

    set(CALIPER_HAVE_NVTX ON)
    set(CALIPER_Nvtx_CMAKE_MSG "Yes, using ${CALI_NVTX_INCLUDE_DIRS}")
  endif()
endif()

if(WITH_CUPTI)
  if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.17)
    find_package(CUDAToolkit REQUIRED)

    if (CUDAToolkit_FOUND)
      set(CALIPER_HAVE_CUPTI ON)
      set(CALIPER_CUpti_CMAKE_MSG "Yes, using ${CUDAToolkit_TARGET_DIR}")
      set(CALI_CUPTI_INCLUDE_DIRS ${CUDAToolkit_INCLUDE_DIRS})
      # Hack for when cupti is in extras/
      find_path(_real_cupti_include_dir
        NAMES cupti.h
        HINTS
          ${CUDAToolkit_TARGET_DIR}/include
          ${CUDAToolkit_TARGET_DIR}/extras/CUPTI/include)
      list(APPEND CALI_CUPTI_INCLUDE_DIRS ${_real_cupti_include_dir})
      list(REMOVE_DUPLICATES CALI_CUPTI_INCLUDE_DIRS)
      # The static library is not always installed
      if (BUILD_SHARED_LIBS OR NOT TARGET CUDA::cupti_static)
        list(APPEND CALIPER_EXTERNAL_LIBS CUDA::cupti)
      else()
        find_library(RT_LIB rt REQUIRED)
        list(APPEND CALIPER_EXTERNAL_LIBS CUDA::cupti_static ${CMAKE_DL_LIBS} ${RT_LIB})
      endif()
    endif()
  else()
    find_package(CUDA REQUIRED)
    include(FindCUPTI)
    if (CUPTI_FOUND)
      set(CALIPER_HAVE_CUPTI TRUE)
      set(CALIPER_CUpti_CMAKE_MSG "Yes, using ${CUPTI_LIBRARY}")
      set(CALI_CUPTI_INCLUDE_DIRS
        ${CUDA_INCLUDE_DIRS}
        ${CUPTI_INCLUDE_DIR})
      list(APPEND CALIPER_EXTERNAL_LIBS ${CUPTI_LIBRARY})
    endif()
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

if (WITH_LIBDW)
  include(FindLibDw)
  if (LIBDW_FOUND)
    list(APPEND CALIPER_EXTERNAL_LIBS ${LIBDW_LIBRARY})
    set(CALIPER_HAVE_LIBDW TRUE)
    set(CALIPER_Libdw_CMAKE_MSG "Yes, using ${LIBDW_LIBRARY}")
  endif()
endif()

# Find libunwind
if (WITH_LIBUNWIND)
  include(FindLibunwind)
  if (LIBUNWIND_FOUND)
    set(CALIPER_HAVE_LIBUNWIND TRUE)
    set(CALIPER_Libunwind_CMAKE_MSG "Yes, using ${LIBUNWIND_LIBRARIES}")
    list(APPEND CALIPER_EXTERNAL_LIBS ${LIBUNWIND_LIBRARIES})
  else()
    message(WARNING "Libunwind support was requested but libunwind was not found!")
  endif()
endif()

# Find Gotcha
if (WITH_GOTCHA)
  if (CALIPER_HAVE_LINUX)
    if (USE_EXTERNAL_GOTCHA)
      find_package(gotcha 2.0)
      if (gotcha_FOUND)
        get_target_property(_gotcha_location gotcha LOCATION)
        set(CALIPER_GOTCHA_CMAKE_MSG "Yes, using ${_gotcha_location}")
        set(CALIPER_HAVE_GOTCHA TRUE)
        list(APPEND CALIPER_EXTERNAL_LIBS ${gotcha_LIBRARIES})
      else()
        message(WARNING "External gotcha was requested but gotcha was not found!")
      endif()
    else()
      set(CALIPER_GOTCHA_CMAKE_MSG "Yes, using internal")
      set(gotcha_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/ext/gotcha/include)
      set(CALIPER_HAVE_GOTCHA TRUE)
      list(APPEND CALIPER_EXTERNAL_SOURCES $<TARGET_OBJECTS:caliper-gotcha>)
    endif()
  else()
    message(WARNING "Gotcha support requested but Gotcha requires Linux, this is ${CMAKE_SYSTEM_NAME}")
  endif()
endif()

if (WITH_ADIAK)
  find_package(adiak)
  if (adiak_FOUND)
    list(GET adiak_LIBRARIES 0 _adiak_target)
    get_target_property(_adiak_location ${_adiak_target} LOCATION)
    set(CALIPER_adiak_CMAKE_MSG "Yes, using ${_adiak_location}")
    set(CALIPER_HAVE_ADIAK TRUE)
    list(APPEND CALIPER_EXTERNAL_LIBS ${adiak_LIBRARIES})
    if (CALIPER_HAVE_LINUX)
      list(APPEND CALIPER_EXTERNAL_LIBS ${CMAKE_DL_LIBS})
    endif()
  else()
    set(CALIPER_HAVE_ADIAK FALSE)
    message(WARNING "Adiak support was requested but Adiak was not found")
  endif()
else()
  set(CALIPER_HAVE_ADIAK FALSE)
endif()

if (WITH_KOKKOS)
  set(CALIPER_HAVE_KOKKOS TRUE)
  set(CALIPER_Kokkos_CMAKE_MSG "Yes")
endif()

# pthread handling
set(THREADS_PREFER_PTHREAD_FLAG On)
find_package(Threads REQUIRED)

if (WITH_OMPT)
  set(CALIPER_HAVE_OMPT TRUE)
  set(CALIPER_OMPT_CMAKE_MSG "Yes")
endif()

# Find MPI
if (WITH_MPI)
  find_package(MPI)
  if (MPI_C_FOUND)
    set(CALIPER_HAVE_MPI TRUE)
    set(CALIPER_MPI_CMAKE_MSG "Yes, using ${MPI_C_LIBRARIES}")

    if (WITH_MPIT)
      message(STATUS "MPIT is currently not supported, disabling.")
      # set(CALIPER_HAVE_MPIT TRUE)
      # set(CALIPER_MPIT_CMAKE_MSG "Yes")
    endif()

    if (CALIPER_HAVE_GOTCHA)
      set(CALIPER_MPIWRAP_USE_GOTCHA TRUE)
      set(CALIPER_MPIWRAP_CMAKE_MSG "Yes, using GOTCHA")
    else()
      set(CALIPER_MPIWRAP_CMAKE_MSG "Yes, using PMPI")
    endif()
  endif()
endif()

if (WITH_ROCPROFILER AND (WITH_ROCTX OR WITH_ROCTRACER))
  message(WARNING "ROCPROFILER and ROCTX/ROCTRACER support are both enabled.\n"
    "This is not recommended, please select either ROCPROFILER or ROCTRACER/ROCTX")
endif()

if (WITH_ROCTX OR WITH_ROCM)
  include(FindRocTX)
  if (ROCTX_FOUND)
    set(CALIPER_HAVE_ROCTX TRUE)
    set(CALIPER_RocTX_CMAKE_MSG "Yes, using ${ROCTX_LIBRARIES}")
    list(APPEND CALIPER_EXTERNAL_LIBS ${ROCTX_LIBRARIES})
  else()
    message(WARNING "RocTX support requested but ROCm was not found!\n"
      "Set ROCM_PATH to installation path and re-run cmake.")
  endif()
endif()

if (WITH_ROCTRACER OR WITH_ROCM)
  include(FindRoctracer)
  if (ROCTRACER_FOUND)
    set(CALIPER_HAVE_ROCTRACER TRUE)
    set(CALIPER_RocTracer_CMAKE_MSG "Yes, using ${ROCTRACER_LIBRARIES}")
    list(APPEND CALIPER_EXTERNAL_LIBS ${ROCTRACER_LIBRARIES})
  else()
    message(WARNING "RocTracer support requested but ROCm was not found!\n"
      "Set ROCM_PATH to installation path and re-run cmake.")
  endif()
endif()

if (WITH_ROCPROFILER)
  find_package(rocprofiler-sdk REQUIRED)
  find_package(rocprofiler-sdk-roctx REQUIRED)
  if (rocprofiler-sdk_FOUND)
    set(CALIPER_HAVE_ROCPROFILER TRUE)
    list(APPEND CALIPER_EXTERNAL_LIBS
      rocprofiler-sdk::rocprofiler-sdk
      rocprofiler-sdk-roctx::rocprofiler-sdk-roctx)
    list(APPEND CALIPER_rocprofiler_CMAKE_MSG "Yes, using ${rocprofiler-sdk_ROOT_DIR}")
  endif()
endif()

if (WITH_CRAYPAT)
  include(FindCrayPAT)
  if (CRAYPAT_FOUND)
    set(CALIPER_HAVE_CRAYPAT TRUE)
    set(CALIPER_CrayPAT_CMAKE_MSG "Yes, using ${CRAYPAT_LIBRARIES}")
    list(APPEND CALIPER_EXTERNAL_LIBS ${CRAYPAT_LIBRARIES})
  else()
    message(WARNING "CrayPAT support requested but CrayPAT was not found!\n"
      "Set CRAYPAT_ROOT to installation path and re-run cmake.")
  endif()
endif()

if(WITH_TAU)
  if (CALIPER_HAVE_MPI)
    find_package(TAU QUIET)
    if (TAU_FOUND)
      set(CALIPER_HAVE_TAU TRUE)
      set(CALIPER_TAU_CMAKE_MSG "Yes, using ${TAU_LIBRARIES}")
      list(APPEND CALIPER_MPI_EXTERNAL_LIBS ${TAU_LIBRARIES})
    else()
      message(WARNING "TAU bindings requested but TAU API was not found!\n"
        "Set TAU_PREFIX to installation path and re-run cmake.")
    endif()
  else()
    message(WARNING "TAU support requires MPI-enabled build!\n")
  endif()
endif()

# Find Python
if (WITH_PYTHON_BINDINGS)
  find_package(Python COMPONENTS Interpreter Development REQUIRED)
  find_package(pybind11 CONFIG REQUIRED)
  if (pybind11_FOUND)
    set(CALIPER_HAVE_PYBIND11 TRUE)
    set(CALIPER_PyBind_CMAKE_MSG "Yes, using pybind ${pybind11_VERSION}")
  else()
    message(WARNING "Python bindings requested but pybind11 was not found")
  endif()
else()
  find_package(Python COMPONENTS Interpreter REQUIRED)
endif()

set(CALI_PYTHON_EXECUTABLE Python::Interpreter)

if (WITH_SAMPLER)
  # Sampler is currently Linux-specific: check for Linux
  if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
    # Linux PC sampler needs -lrt
    find_library(RT_LIBRARY
      rt)
    if (NOT ${RT_LIBRARY} MATCHES RT_LIBRARY-NOTFOUND)
      set(CALIPER_HAVE_SAMPLER TRUE)
      set(CALIPER_Sampler_CMAKE_MSG "Yes")
      list(APPEND CALIPER_EXTERNAL_LIBS ${RT_LIBRARY})
    endif()
  else()
    message(WARNING "Sampler is not supported on ${CMAKE_SYSTEM_NAME}")
  endif()
endif()

if (WITH_PCP)
  find_library(PCP_LIBRARY
    pcp)
  if (NOT ${PCP_LIBRARY} MATCHES PCP_LIBRARY-NOTFOUND)
    set(CALIPER_HAVE_PCP TRUE)
    set(CALIPER_PCP_CMAKE_MSG "Yes, using ${PCP_LIBRARY}")
    list(APPEND CALIPER_EXTERNAL_LIBS ${PCP_LIBRARY})
  endif()
endif()

# Find VARIORUM
if (WITH_VARIORUM)
  include(FindVariorum)
  if (VARIORUM_FOUND)
    set(CALIPER_HAVE_VARIORUM TRUE)
    set(CALIPER_variorum_CMAKE_MSG "Yes, using ${VARIORUM_LIBRARIES}")
    list(APPEND CALIPER_EXTERNAL_LIBS ${VARIORUM_LIBRARIES})
  else()
    message(WARNING "Variorum support was requested but Variorum was not found!\n"
  "Set VARIORUM_PREFIX to the Variorum installation path and re-run cmake.")
  endif()
endif()

if (WITH_UMPIRE)
  find_package(umpire REQUIRED)
  set(CALIPER_HAVE_UMPIRE TRUE)
  get_target_property(_umpire_location umpire LOCATION)
  set(CALIPER_umpire_CMAKE_MSG "Yes, using ${_umpire_location}")
  list(APPEND CALIPER_EXTERNAL_LIBS umpire)
endif()

if (WITH_LDMS)
  include(FindLDMS)
  if (LDMS_FOUND)
     set(CALIPER_HAVE_LDMS TRUE)
     set(CALIPER_LDMS_CMAKE_MSG "Yes, using ${LDMS_LIBLDMS} ${LDMS_LIBSTREAM}")
     list(APPEND CALIPER_EXTERNAL_LIBS ${LDMS_LIBLDMS} ${LDMS_LIBSTREAM})
     message("Include Directory: ${LDMS_INCLUDE_DIRS}")
     message("Library Names: ${LDMS_LIBLDMS} ${LDMS_LIBSTREAM}")
   endif()
endif()

if (BUILD_TESTING)
  set(CALIPER_BUILD_TESTING TRUE)
endif()

# Create a config header file
configure_file(
  ${PROJECT_SOURCE_DIR}/caliper-config.h.in
  ${PROJECT_BINARY_DIR}/include/caliper/caliper-config.h)

# Include and install header files
include_directories(${PROJECT_BINARY_DIR}/include)
include_directories(include)

if (WITH_PYTHON_BINDINGS AND BUILD_TESTING)
  set(PYPATH_TESTING "" CACHE INTERNAL "")
endif()

add_subdirectory(ext)
add_subdirectory(src)

add_subdirectory(examples/apps EXCLUDE_FROM_ALL)

if (BUILD_TESTING)
  add_subdirectory(test)
endif()
if (BUILD_DOCS)
  add_subdirectory(doc EXCLUDE_FROM_ALL)
endif()

if (INSTALL_HEADERS)
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h")
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp")

  install(FILES ${PROJECT_BINARY_DIR}/include/caliper/caliper-config.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/caliper)

  install(
    DIRECTORY   src/interface/c_fortran/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/caliper
    FILES_MATCHING PATTERN "*.h")

  if (CALIPER_HAVE_FORTRAN)
    install(
      DIRECTORY   ${CMAKE_Fortran_MODULE_DIRECTORY}/
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/caliper/fortran
      FILES_MATCHING PATTERN "*.mod")
  endif()

  install(
    DIRECTORY
      "python/caliper-reader/caliperreader"
    DESTINATION
      "${CMAKE_INSTALL_LIBDIR}/caliper"
    FILES_MATCHING
      PATTERN "*.py"
  )

  install(
    PROGRAMS
      "python/cali2traceevent.py"
      "python/cali2traceevent"
    DESTINATION
      ${CMAKE_INSTALL_BINDIR}
  )
endif()

if (INSTALL_CONFIG)
  # Create pkg-config .pc file
  set(PKG_CONFIG_INCLUDEDIR "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
  set(PKG_CONFIG_LIBDIR "${CMAKE_INSTALL_FULL_LIBDIR}")
  set(PKG_CONFIG_LIBS "-L\${libdir} -lcaliper")
  set(PKG_CONFIG_CFLAGS "-I\${includedir}")

  configure_file(
    ${PROJECT_SOURCE_DIR}/caliper.pc.in
    ${PROJECT_BINARY_DIR}/caliper.pc)

  include(CMakePackageConfigHelpers)

  write_basic_package_version_file(
      "${CMAKE_CURRENT_BINARY_DIR}/caliper-config-version.cmake"
    VERSION
      ${LIBTOOL_INTERFACE}.${LIBTOOL_REVISION}.${LIBTOOL_AGE}
    COMPATIBILITY
      SameMajorVersion
  )

  set(caliper_INSTALL_INCLUDE_DIR "include/")

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/caliper-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/caliper-config.cmake"
    INSTALL_DESTINATION
      ${CMAKE_INSTALL_DATAROOTDIR}/cmake/caliper
    PATH_VARS
      caliper_INSTALL_INCLUDE_DIR
  )

  install(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/caliper-config-version.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/caliper-config.cmake"
    DESTINATION
      "${CMAKE_INSTALL_DATAROOTDIR}/cmake/caliper"
  )

  install(
    EXPORT
      caliper-targets
    DESTINATION
      "${CMAKE_INSTALL_DATAROOTDIR}/cmake/caliper"
  )

  install(
    FILES
      ${PROJECT_BINARY_DIR}/caliper.pc
    DESTINATION
      ${CMAKE_INSTALL_LIBDIR}/pkgconfig
  )
endif()

#
# Print config summary
#

message(STATUS "Caliper configuration summary:")

message(STATUS "Caliper version           : ${CALIPER_VERSION}")
message(STATUS "Build type                : ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler                  : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} (${CMAKE_CXX_COMPILER})")
message(STATUS "Python interpreter        : ${Python_EXECUTABLE}")
message(STATUS "System                    : ${CMAKE_SYSTEM} (${CMAKE_SYSTEM_PROCESSOR})")
message(STATUS "Architecture              : ${CALIPER_HAVE_ARCH}")
message(STATUS "Install dir               : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Build shared libs         : ${BUILD_SHARED_LIBS}")
message(STATUS "Build Caliper tools       : ${WITH_TOOLS}")
message(STATUS "Build tests               : ${BUILD_TESTING}")

set(CALIPER_MODULES
  Fortran
  PyBind
  cpuinfo
  memusage
  adiak
  GOTCHA
  PAPI
  Libdw
  Libpfm
  Libunwind
  Sampler
  MPI
  MPIWRAP
  OMPT
  Nvtx
  CUpti
  Kokkos
  RocTracer
  RocTX
  rocprofiler
  TAU
  VTune
  PCP
  histogram
  variorum
  umpire
  CrayPAT
  LDMS)

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
