set(CALIPER_MONITOR_SOURCES
  LoopMonitor.cpp
  RegionMonitor.cpp)

add_service_sources(${CALIPER_MONITOR_SOURCES})

add_caliper_service("loop_monitor")
add_caliper_service("region_monitor")
# add_caliper_service("thread_monitor")
