include_directories(${CUPTI_INCLUDE_DIR})
include_directories(${CUDA_INCLUDE_DIRS})

set(CALIPER_CUPTITRACE_SOURCES
  CuptiTrace.cpp)

add_library(caliper-cuptitrace OBJECT ${CALIPER_CUPTITRACE_SOURCES})

add_service_objlib("caliper-cuptitrace")
add_caliper_service("cuptitrace CALIPER_HAVE_CUPTI")
