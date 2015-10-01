include_directories("../common")
include_directories("../caliper")

# A macro to include service module source files in the caliper runtime lib

macro(add_service_sources)
  file(RELATIVE_PATH _relpath "${CMAKE_SOURCE_DIR}/src/services" "${CMAKE_CURRENT_SOURCE_DIR}")
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

# A macro to include service modules as object libs in the caliper runtime lib.
# Used when service subdirectories needs additional includes etc. 

macro(add_service_objlib)
  if(${BUILD_SHARED_LIBS})
    set_property(TARGET "${ARGN}" PROPERTY POSITION_INDEPENDENT_CODE TRUE)
  endif()
  list(APPEND CALIPER_SERVICES_LIBS "$<TARGET_OBJECTS:${ARGN}>")
  set(CALIPER_SERVICES_LIBS ${CALIPER_SERVICES_LIBS} PARENT_SCOPE)
endmacro()

# Service subdirectories

if (LIBUNWIND_FOUND)
  add_subdirectory(callpath)
endif()
if (PAPI_FOUND)
  add_subdirectory(papi)
endif()
add_subdirectory(debug)
add_subdirectory(event)
add_subdirectory(pthread)
add_subdirectory(recorder)
add_subdirectory(timestamp)
add_subdirectory(statistics)
add_subdirectory(trace)
if (MPI_C_FOUND)
  add_subdirectory(mpiwrap)
endif()
if (OMPT_FOUND)
  add_subdirectory(ompt)
endif()

add_service_sources(Services.cpp)

# Build service management classes 

add_library(caliper-services OBJECT ${CALIPER_SERVICES_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-services PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()

# Export service object libs to parent scope

set(CALIPER_SERVICES_LIBS ${CALIPER_SERVICES_LIBS} PARENT_SCOPE)
