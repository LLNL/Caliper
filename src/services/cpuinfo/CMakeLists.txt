set(CALIPER_CPUINFO_SOURCES
  CpuInfo.cpp)

add_service_sources(${CALIPER_CPUINFO_SOURCES})
add_caliper_service("cpuinfo")
