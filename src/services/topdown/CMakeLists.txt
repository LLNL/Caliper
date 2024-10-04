set(CALIPER_TOPDOWN_SOURCES
  IntelTopdown.cpp
  TopdownCalulator.cpp
  HaswellTopdown.cpp
  SapphireRapidsTopdown.cpp)

add_library(caliper-topdown OBJECT ${CALIPER_TOPDOWN_SOURCES})

add_service_objlib("caliper-topdown")
add_caliper_service("topdown CALIPER_HAVE_PAPI")
