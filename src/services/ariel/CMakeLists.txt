include_directories(${ARIELAPI_INCLUDE_DIR})

set(CALIPER_ARIEL_SOURCES
  ArielBindings.cpp)

add_library(caliper-ariel OBJECT ${CALIPER_ARIEL_SOURCES})

add_service_objlib("caliper-ariel")
add_caliper_service("ariel")
