include_directories(${TAU_INCLUDE_DIR})

set(CALIPER_TAU_SOURCES
  tau.cpp)

add_library(caliper-tau OBJECT ${CALIPER_TAU_SOURCES})

add_service_objlib("caliper-tau")
add_caliper_service("tau CALIPER_HAVE_TAU")
