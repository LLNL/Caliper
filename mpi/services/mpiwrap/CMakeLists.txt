include_directories(${GOTCHA_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../) # for MpiEvents.h

set(WRAP_FLAGS "-w")

if (CALIPER_MPIWRAP_USE_GOTCHA)
  set(WRAP_FLAGS "${WRAP_FLAGS}G")
endif()

set(WRAP ${CMAKE_CURRENT_SOURCE_DIR}/wrap.py)
include(${PROJECT_SOURCE_DIR}/cmake/WrapConfig.cmake)

add_wrapped_file(Wrapper.cpp Wrapper.w)

set(CALIPER_PMPI_SOURCES
    Wrapper.cpp)

add_library(caliper-pmpi OBJECT ${CALIPER_PMPI_SOURCES})

add_mpi_service_objlib(caliper-pmpi)
