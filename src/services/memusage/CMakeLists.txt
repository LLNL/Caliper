set(CALIPER_MEMUSAGE_SOURCES
  MemUsageService.cpp)

add_service_sources(${CALIPER_MEMUSAGE_SOURCES})
add_caliper_service("memusage")
