set(CALIPER_CUPTI_SOURCES
  Cupti.cpp
  CuptiEventSampling.cpp
  CuptiTrace.cpp)

add_library(caliper-cupti OBJECT ${CALIPER_CUPTI_SOURCES})
target_include_directories(caliper-cupti PRIVATE ${CALI_CUPTI_INCLUDE_DIRS})

add_service_objlib("caliper-cupti")

add_caliper_service("cupti CALIPER_HAVE_CUPTI")
add_caliper_service("cuptitrace CALIPER_HAVE_CUPTI")