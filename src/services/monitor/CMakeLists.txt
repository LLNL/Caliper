set(CALIPER_MONITOR_SOURCES
  ThreadMonitor.cpp)

add_service_sources(${CALIPER_MONITOR_SOURCES})
add_caliper_service("monitor")
