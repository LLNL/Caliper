include_directories(${MPI_CXX_INCLUDE_PATH})

set(CALIPER_MPI_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(CALIPER_MPI_SOURCES
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

if (CALIPER_HAVE_TAU)
  add_subdirectory(services/tau)
endif()

add_subdirectory(controllers)

if (CALIPER_HAVE_MPIT)
  # add_subdirectory(services/mpit)
  message(WARNING "MPIT option is currently not supported")
endif()

add_library(caliper-mpi-obj OBJECT ${CALIPER_MPI_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-mpi-obj PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()

set(CALIPER_MPI_OBJS
  $<TARGET_OBJECTS:caliper-mpi-common>
  $<TARGET_OBJECTS:caliper-mpi-controllers>
  $<TARGET_OBJECTS:caliper-mpi-obj>
  ${CALIPER_MPI_LIBS})

#   libcaliper-mpi is deprecated and exists for backward compatibility only.
# Use the combined libcaliper instead.
add_library(caliper-mpi ${CALIPER_MPI_OBJS})

set_target_properties(caliper-mpi PROPERTIES SOVERSION ${CALIPER_MAJOR_VERSION})
set_target_properties(caliper-mpi PROPERTIES VERSION ${CALIPER_VERSION})

foreach (_extlib ${CALIPER_MPI_EXTERNAL_LIBS})
  target_link_libraries(caliper-mpi PRIVATE ${_extlib})
endforeach()

target_link_libraries(caliper-mpi PUBLIC caliper-serial)
target_link_libraries(caliper-mpi PRIVATE ${MPI_CXX_LIBRARIES})

# Add mpi objects and dependencies to global list for combined caliper lib
list(APPEND CALIPER_ALL_OBJS
  ${CALIPER_MPI_OBJS})
list(APPEND CALIPER_ALL_EXTERNAL_LIBS
  ${CALIPER_MPI_EXTERNAL_LIBS}
  ${MPI_CXX_LIBRARIES})

set(CALIPER_ALL_OBJS ${CALIPER_ALL_OBJS} PARENT_SCOPE)
set(CALIPER_ALL_EXTERNAL_LIBS ${CALIPER_ALL_EXTERNAL_LIBS} PARENT_SCOPE)

install(TARGETS caliper-mpi
  EXPORT caliper
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
