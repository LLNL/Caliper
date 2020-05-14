set(CALIPER_ALLOC_SOURCES
    AllocService.cpp)

add_service_sources(${CALIPER_ALLOC_SOURCES})
add_caliper_service("alloc")
