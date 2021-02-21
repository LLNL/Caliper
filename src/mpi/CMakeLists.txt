include_directories(${MPI_CXX_INCLUDE_PATH})

set(CALIPER_MPI_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(CALIPER_MPI_SOURCES
  CollectiveOutputChannel.cpp
  aggregate_over_mpi.cpp
  collective_flush.cpp
  mpi_flush.cpp
  mpi_machine.cpp
  mpi_setup.cpp)

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
  list(APPEND CALIPER_MPI_SERVICES_LIBS "$<TARGET_OBJECTS:${ARGN}>")
  set(CALIPER_MPI_SERVICES_LIBS ${CALIPER_MPI_SERVICES_LIBS} PARENT_SCOPE)
endmacro()

add_subdirectory(services/mpiwrap)
add_subdirectory(services/mpireport)

if (CALIPER_HAVE_TAU)
  add_subdirectory(services/tau)
endif()

add_subdirectory(controllers)

if (CALIPER_HAVE_MPIT)
  # add_subdirectory(services/mpit)
  message(WARNING "MPIT option is currently not supported")
endif()

add_library(caliper-mpi OBJECT ${CALIPER_MPI_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-mpi PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()

# Spot controller needs adiak
if (${CALIPER_HAVE_ADIAK})
  target_include_directories(caliper-mpi PRIVATE ${adiak_INCLUDE_DIRS})
endif()

# Add mpi objects and dependencies to global list for combined caliper lib
list(APPEND CALIPER_ALL_OBJS
  $<TARGET_OBJECTS:caliper-mpi>
  ${CALIPER_MPI_SERVICES_LIBS})
list(APPEND CALIPER_ALL_EXTERNAL_LIBS
  ${CALIPER_MPI_EXTERNAL_LIBS}
  ${MPI_CXX_LIBRARIES})

set(CALIPER_ALL_OBJS ${CALIPER_ALL_OBJS} PARENT_SCOPE)
set(CALIPER_ALL_EXTERNAL_LIBS ${CALIPER_ALL_EXTERNAL_LIBS} PARENT_SCOPE)
