set(CALIPER_ALLOC_SOURCES
    AllocService.cpp
    AllocSizeService.cpp)

add_service_sources(${CALIPER_ALLOC_SOURCES})
add_caliper_service("alloc")
add_caliper_service("allocsize")
