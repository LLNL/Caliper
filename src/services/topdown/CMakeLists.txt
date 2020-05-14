set(CALIPER_TOPDOWN_SOURCES
  IntelTopdown.cpp)

add_service_sources(${CALIPER_TOPDOWN_SOURCES})
add_caliper_service("topdown CALIPER_HAVE_PAPI")
