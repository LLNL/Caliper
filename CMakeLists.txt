#
# Caliper
#

cmake_minimum_required(VERSION 3.0)
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

if (NOT BUILD_SHARED_LIBS)
  # Try to find static libs first for static builds
  list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 .a)
endif()

# RPATH setup. By default, rpath everything.
option(CMAKE_INSTALL_RPATH_USE_LINK_PATH "Add rpath for all dependencies" TRUE)

option(WITH_TOOLS     "Build Caliper tools" TRUE)
option(WITH_TEST_APPS "Build Caliper test apps" TRUE)
option(WITH_DOCS      "Build Caliper documentation" FALSE)

option(WITH_CUDA      "Enable Caliper CUDA services" FALSE)
option(WITH_NETOUT    "Enable netout service (requires curl)" FALSE)
option(WITH_PAPI      "Enable PAPI hardware counter service (requires papi)" TRUE)
option(WITH_CALLPATH  "Enable callpath service (requires libunwind)" TRUE)
option(WITH_MPI       "Enable MPI" TRUE)

# configure testing explicitly rather than with include(CTest) - avoids some clutter
option(BUILD_TESTING  "Build continuous integration app and unit tests" ON)

if (BUILD_TESTING)
  enable_testing()
endif()

# the RPATH to be used when installing, but only if it's not a system directory
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES ${CMAKE_INSTALL_FULL_LIBDIR} isSystemDir)
if("${isSystemDir}" STREQUAL "-1")
   set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
endif("${isSystemDir}" STREQUAL "-1")

if(WITH_CUDA)
   find_package(CUDA)
   include_directories(${CUDA_INCLUDE_DIRS})
   find_library(NVVP_LIBRARY NAME libnvToolsExt.so PATHS ${CUDA_TOOLKIT_ROOT_DIR}/lib64 ${CUDA_TOOLKIT_ROOT_DIR}/lib)
   set(CALIPER_HAVE_NVVP ON)
   list(APPEND CALIPER_EXTERNAL_LIBS ${NVVP_LIBRARY})
endif()

if(CALIPER_HAVE_TAU)
    find_library(tau_lib libTAU.so)
    list(APPEND CALIPER_EXTERNAL_LIBS ${tau_lib})
endif()
if(CALIPER_HAVE_SCOREP)
    #find_library(scorep_lib libscorep_measurement.so -lscorep_adapter_compiler_event -lscorep_adapter_mpi_event -lscorep_measurement -lscorep_adapter_utils -lotf2 -lcube4w -lcubewriter4 -lscorep_adapter_compiler_mgmt -lscorep_adapte
#r_memory_mgmt -lscorep_adapter_mpi_mgmt -lscorep_mpp_mpi -lscorep_online_access_mpp_mpi -lscorep_thread_mockup -lscorep_mutex_mockup -lscorep_alloc_metric -lpapi -ldl -lrt -lunwind)
    find_library(scorep_lib1 libscorep_adapter_compiler_event.so)
    find_library(scorep_lib2 libscorep_adapter_mpi_event.so)
    find_library(scorep_lib3 libscorep_measurement.so)
    find_library(scorep_lib4 libscorep_adapter_utils.so)
    find_library(scorep_lib5 libotf2.so)
    find_library(scorep_lib6 libcube4w.so)
    find_library(scorep_lib7 libcubewriter4.so)
    find_library(scorep_lib8 libscorep_adapter_compiler_mgmt.so)
    find_library(scorep_lib9 libscorep_adapter_memory_mgmt.so)
    find_library(scorep_lib10 libscorep_adapter_mpi_mgmt.so)
    find_library(scorep_lib11 libscorep_mpp_mpi.so)
    find_library(scorep_lib12 libscorep_online_access_mpp_mpi.so)
    find_library(scorep_lib13 libscorep_thread_mockup.so)
    find_library(scorep_lib14 libscorep_mutex_mockup.so)
    find_library(scorep_lib15 libscorep_alloc_metric.so)
    find_library(scorep_lib16 libpapi.so)
    find_library(scorep_lib17 libdl.so)
    find_library(scorep_lib18 librt.so)
    find_library(scorep_lib19 libunwind.so)
    find_library(scorep_lib20 libscorep_adapter_user_event.so)
    find_library(scorep_lib21 libscorep_adapter_user_mgmt.so)
    find_library(scorep_lib22 libscorep_adapter_mpi_event.so)
    find_library(scorep_lib23 libscorep_adapter_mpi_mgmt.so)
    find_library(scorep_lib24 libscorep_measurement.so)
    list(APPEND CALIPER_EXTERNAL_LIBS ${scorep_lib1} ${scorep_lib2} ${scorep_lib3} ${scorep_lib4} ${scorep_lib5} ${scorep_lib6} ${scorep_lib7} ${scorep_lib8} ${scorep_lib9} ${scorep_lib10} ${scorep_lib11} ${scorep_lib12} ${scorep_lib13} ${scorep_lib14} ${scorep_lib15} ${scorep_lib16} ${scorep_lib17} ${scorep_lib18} ${scorep_lib19} ${scorep_lib20} ${scorep_lib21} ${scorep_lib22} ${scorep_lib23})
endif()

# Find libunwind
if (WITH_CALLPATH)
  include(FindLibunwind)
  if (LIBUNWIND_FOUND)
    set(CALIPER_HAVE_LIBUNWIND TRUE)
    list(APPEND CALIPER_EXTERNAL_LIBS ${LIBUNWIND_LIBRARY})
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

# Find Mitos

# find_package(Mitos QUIET)
# if(Mitos_FOUND)
#   find_package(Dyninst REQUIRED SymtabAPI InstructionAPI ParseAPI)
#   set(CALIPER_HAVE_MITOS TRUE)
#   list(APPEND CALIPER_EXTERNAL_LIBS mitos hwloc symtabAPI instructionAPI parseAPI)
#   message(STATUS "Mitos found")
# endif()

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

if (WITH_MPI)
  find_package(MPI)
  if (MPI_C_FOUND)
    set(CALIPER_HAVE_MPI TRUE)
  endif()
endif()

# Find Python

find_package(PythonInterp REQUIRED)

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

install(FILES ${PROJECT_BINARY_DIR}/caliper-config.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/caliper)

install(FILES ${PROJECT_BINARY_DIR}/caliper.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

add_subdirectory(ext)
add_subdirectory(src)

if (WITH_TEST_APPS)
  add_subdirectory(test)
endif()
if (WITH_DOCS)
  add_subdirectory(doc)
endif()

# Install exports
#install(EXPORT caliper DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
