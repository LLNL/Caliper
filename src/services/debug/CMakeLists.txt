set(CALIPER_DEBUG_SOURCES
    Debug.cpp)

add_service_sources(${CALIPER_DEBUG_SOURCES})
add_caliper_service("debug")
