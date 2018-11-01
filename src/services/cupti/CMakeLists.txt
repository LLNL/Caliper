include_directories(${CUPTI_INCLUDE_DIR})
include_directories(${CUDA_INCLUDE_DIRS})

set(CALIPER_CUPTI_SOURCES
  CuptiEventSampling.cpp
  Cupti.cpp)

add_library(caliper-cupti OBJECT ${CALIPER_CUPTI_SOURCES})

add_service_objlib("caliper-cupti")
add_caliper_service("cupti CALIPER_HAVE_CUPTI")
