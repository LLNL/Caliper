set(CALIPER_MEMUSAGE_SOURCES
  MemStatService.cpp)

add_service_sources(${CALIPER_MEMUSAGE_SOURCES})
add_caliper_service("memstat")
