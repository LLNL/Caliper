# A macro to include service module source files in the caliper runtime lib
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(CALIPER_SERVICES_SRC_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

macro(add_service_sources)
  file(RELATIVE_PATH _relpath "${CALIPER_SERVICES_SRC_ROOT_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  foreach(_src ${ARGN})
    if (_relpath)
      list(APPEND CALIPER_SERVICES_SOURCES "${_relpath}/${_src}")
    else()
      list(APPEND CALIPER_SERVICES_SOURCES "${_src}")
    endif()
  endforeach()
  if (_relpath)
    set(CALIPER_SERVICES_SOURCES ${CALIPER_SERVICES_SOURCES} PARENT_SCOPE)
  endif()
endmacro()

macro(add_caliper_service)
  foreach (_srv ${ARGN})
    string(REPLACE " " ":" NEW_SERVICE ${_srv})
    list(APPEND CALIPER_SERVICE_NAMES ${NEW_SERVICE})
  endforeach()
  set(CALIPER_SERVICE_NAMES ${CALIPER_SERVICE_NAMES} PARENT_SCOPE)
endmacro()
# A macro to include service modules as object libs in the caliper runtime lib.
# Used when service subdirectories needs additional includes etc.

macro(add_service_objlib)
  if(${BUILD_SHARED_LIBS})
    set_property(TARGET "${ARGN}" PROPERTY POSITION_INDEPENDENT_CODE TRUE)
  endif()
  target_compile_options("${ARGN}" PRIVATE ${Wall_flag})
  list(APPEND CALIPER_SERVICES_LIBS "$<TARGET_OBJECTS:${ARGN}>")
  set(CALIPER_SERVICES_LIBS ${CALIPER_SERVICES_LIBS} PARENT_SCOPE)
endmacro()

# Service subdirectories

add_subdirectory(alloc)
if (CALIPER_HAVE_ADIAK)
  add_subdirectory(adiak)
endif()
add_subdirectory(aggregate)
if (CALIPER_HAVE_LIBUNWIND)
  add_subdirectory(callpath)
endif()
if (CALIPER_HAVE_CPUINFO)
  add_subdirectory(cpuinfo)
endif()
if (CALIPER_HAVE_MEMUSAGE)
  add_subdirectory(memusage)
endif()
if (CALIPER_HAVE_PAPI)
  add_subdirectory(papi)
  add_subdirectory(topdown)
endif()
if (CALIPER_HAVE_LIBPFM)
  add_subdirectory(libpfm)
endif()
if (CALIPER_HAVE_NVTX)
  add_subdirectory(nvtx)
endif()
if (CALIPER_HAVE_CUPTI)
  add_subdirectory(cupti)
endif()
add_subdirectory(debug)
add_subdirectory(env)
add_subdirectory(event)
if (CALIPER_HAVE_KOKKOS)
  add_subdirectory(kokkos)
endif()
add_subdirectory(monitor)
add_subdirectory(textlog)
if (CALIPER_HAVE_GOTCHA)
  add_subdirectory(io)
  add_subdirectory(pthread)
  add_subdirectory(sysalloc)
endif()
# if (CALIPER_HAVE_SOS)
#   add_subdirectory(sos)
# endif()
add_subdirectory(recorder)
add_subdirectory(report)
if (CALIPER_HAVE_SAMPLER)
  add_subdirectory(sampler)
endif()
add_subdirectory(timestamp)
add_subdirectory(statistics)
add_subdirectory(spot)
if (CALIPER_HAVE_LIBDW)
  add_subdirectory(symbollookup)
endif()
add_subdirectory(trace)
if (CALIPER_HAVE_OMPT)
  add_subdirectory(ompt)
endif()
add_subdirectory(validator)
if (CALIPER_HAVE_VTUNE)
  add_subdirectory(vtune)
endif()
if (CALIPER_HAVE_PCP)
  add_subdirectory(pcp)
endif()
if (CALIPER_HAVE_ROCM)
  add_subdirectory(rocm)
endif()

if (CALIPER_BUILD_TESTING)
  add_subdirectory(templates)
endif()

add_service_sources(Services.cpp)

configure_file(
   ${CMAKE_CURRENT_SOURCE_DIR}/gen_services_inc.py.in
   ${CMAKE_CURRENT_BINARY_DIR}/gen_services_inc.py
)

add_custom_command(
  OUTPUT services.inc.cpp
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/gen_services_inc.py
  COMMAND ${CALI_PYTHON_EXECUTABLE} gen_services_inc.py
  COMMENT "Generating services.inc.cpp"
)

add_custom_target(
  gen-services ALL
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/services.inc.cpp
)

# Build service management classes

add_library(caliper-services OBJECT ${CALIPER_SERVICES_SOURCES})

target_compile_options(caliper-services PRIVATE ${Wall_flag})

add_dependencies(caliper-services gen-services)

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-services PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()

set(CALIPER_SERVICES_LIBS ${CALIPER_SERVICES_LIBS} PARENT_SCOPE)
