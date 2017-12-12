set(CALIPER_MPI_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(CALIPER_MPI_SOURCES
  MpiEvents.cpp
  setup_mpi.cpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}) # Find MpiEvents.h

macro(add_mpi_service_sources)
  file(RELATIVE_PATH _relpath "${CALIPER_MPI_ROOT_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  foreach(_src ${ARGN})
    if (_relpath)
      list(APPEND CALIPER_MPI_SOURCES "${_relpath}/${_src}")
    else()
      list(APPEND CALIPER_MPI_SOURCES "${_src}")
    endif()
  endforeach()
  if (_relpath)
    set(CALIPER_MPI_SOURCES ${CALIPER_MPI_SOURCES} PARENT_SCOPE)
  endif()
endmacro()

macro(add_mpi_service_objlib)
  if(${BUILD_SHARED_LIBS})
    set_property(TARGET "${ARGN}" PROPERTY POSITION_INDEPENDENT_CODE TRUE)
  endif()
  list(APPEND CALIPER_MPI_LIBS "$<TARGET_OBJECTS:${ARGN}>")
  set(CALIPER_MPI_LIBS ${CALIPER_MPI_LIBS} PARENT_SCOPE)
endmacro()

add_subdirectory(services/mpiwrap)
add_subdirectory(services/mpireport)

if (CALIPER_HAVE_MPIT)
  add_subdirectory(services/mpit)
endif()

add_library(caliper-mpi
  ${CALIPER_MPI_SOURCES} 
  ${CALIPER_MPI_LIBS})

set_target_properties(caliper-mpi PROPERTIES SOVERSION ${CALIPER_MAJOR_VERSION})
set_target_properties(caliper-mpi PROPERTIES VERSION ${CALIPER_VERSION})

target_link_libraries(caliper-mpi caliper-mpi-common caliper)

install(TARGETS caliper-mpi
  EXPORT caliper
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
