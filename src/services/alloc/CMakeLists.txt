set(CALIPER_ALLOC_SOURCES
    AllocService.cpp
    AllocStatsService.cpp)

add_service_sources(${CALIPER_ALLOC_SOURCES})
add_caliper_service("alloc")
add_caliper_service("allocstats")
