include_directories(${ITT_INCLUDE_DIR})

set(CALIPER_VTUNE_SOURCES
  VTuneBindings.cpp)

add_library(caliper-vtune OBJECT ${CALIPER_VTUNE_SOURCES})

add_service_objlib("caliper-vtune")
add_caliper_service("vtune CALIPER_HAVE_VTUNE")
