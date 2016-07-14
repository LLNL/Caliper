set(CALIPER_STATISTICS_SOURCES
    Statistics.cpp)

add_service_sources(${CALIPER_STATISTICS_SOURCES})
add_caliper_service("statistics")
