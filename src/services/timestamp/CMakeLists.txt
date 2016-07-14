set(CALIPER_TIMESTAMP_SOURCES
    Timestamp.cpp)

add_service_sources(${CALIPER_TIMESTAMP_SOURCES})
add_caliper_service("timestamp")
