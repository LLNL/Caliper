include_directories(${gotcha_INCLUDE_DIRS})

set(CALIPER_CUDAWRAP_SOURCES
  CudawrapService.cpp)

add_library(caliper-cudawrap OBJECT ${CALIPER_CUDAWRAP_SOURCES})

add_service_objlib("caliper-cudawrap")
add_caliper_service("cudawrap CALIPER_HAVE_GOTCHA")
