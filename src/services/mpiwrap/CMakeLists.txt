include_directories(${MPI_C_INCLUDE_PATH})

set(WRAP ${CMAKE_CURRENT_SOURCE_DIR}/wrap.py)
include(${PROJECT_SOURCE_DIR}/cmake/WrapConfig.cmake)

add_wrapped_file(Wrapper.cpp Wrapper.w)

set(CALIPER_MPI_SOURCES
    MpiWrap.cpp)
set(CALIPER_MPIWRAP_SOURCES
    Wrapper.cpp)

set(CALIPER_MPIWRAP_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

macro(add_mpi_service_sources)
  file(RELATIVE_PATH _relpath "${CALIPER_MPIWRAP_ROOT_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  foreach(_src ${ARGN})
    if (_relpath)
      list(APPEND CALIPER_MPIWRAP_SOURCES "${_relpath}/${_src}")
    else()
      list(APPEND CALIPER_MPIWRAP_SOURCES "${_src}")
    endif()
  endforeach()
  if (_relpath)
    set(CALIPER_MPIWRAP_SOURCES ${CALIPER_MPIWRAP_SOURCES} PARENT_SCOPE)
  endif()
endmacro()

if (CALIPER_HAVE_MPIT)
  add_subdirectory(mpit)
endif()

add_service_sources(${CALIPER_MPI_SOURCES})

add_library(caliper-mpiwrap ${CALIPER_MPIWRAP_SOURCES})

target_link_libraries(caliper-mpiwrap caliper-common)
target_link_libraries(caliper-mpiwrap caliper)
target_link_libraries(caliper-mpiwrap ${MPI_C_LIBRARIES} ${MPI_CXX_LIBRARIES})

install(TARGETS caliper-mpiwrap
  EXPORT caliper
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_caliper_service("mpi CALIPER_HAVE_MPI")
