include_directories(${gotcha_INCLUDE_DIRS})

set(WRAP_FLAGS "-w")

if (CALIPER_MPIWRAP_USE_GOTCHA)
  set(WRAP_FLAGS "${WRAP_FLAGS}G")
endif()

set(WRAP ${CMAKE_CURRENT_SOURCE_DIR}/wrap.py)
include(${PROJECT_SOURCE_DIR}/cmake/WrapConfig.cmake)

add_wrapped_file(Wrapper.cpp Wrapper.w)

set(CALIPER_MPIWRAP_SOURCES
  MpiTracing.cpp
  MpiWrap.cpp
  Wrapper.cpp)

add_library(caliper-mpiwrap OBJECT ${CALIPER_MPIWRAP_SOURCES})

add_mpi_service_objlib(caliper-mpiwrap)
