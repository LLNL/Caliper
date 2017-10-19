set(CALIPER_MPIWRAP_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

macro(add_mpi_service_sources)
  file(RELATIVE_PATH _relpath "${CALIPER_MPIWRAP_ROOT_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  foreach(_src ${ARGN})
    if (_relpath)
      list(APPEND CALIPER_MPI_SERVICE_SOURCES "${_relpath}/${_src}")
    else()
      list(APPEND CALIPER_MPI_SERVICE_SOURCES "${_src}")
    endif()
  endforeach()
  if (_relpath)
    set(CALIPER_MPI_SERVICE_SOURCES ${CALIPER_MPI_SERVICE_SOURCES} PARENT_SCOPE)
  endif()
endmacro()

macro(add_mpi_service_objlib)
  if(${BUILD_SHARED_LIBS})
    set_property(TARGET "${ARGN}" PROPERTY POSITION_INDEPENDENT_CODE TRUE)
  endif()
  list(APPEND CALIPER_MPI_SERVICES_LIBS "$<TARGET_OBJECTS:${ARGN}>")
  set(CALIPER_MPI_SERVICES_LIBS ${CALIPER_MPI_SERVICES_LIBS} PARENT_SCOPE)
endmacro()

add_subdirectory(mpiwrap)
add_subdirectory(mpireport)

if (CALIPER_HAVE_MPIT)
  add_subdirectory(mpit)
endif()

add_library(caliper-mpiwrap 
  ${CALIPER_MPI_SERVICE_SOURCES} 
  ${CALIPER_MPI_SERVICES_LIBS})

target_link_libraries(caliper-mpiwrap caliper-mpi caliper)

install(TARGETS caliper-mpiwrap
  EXPORT caliper
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
