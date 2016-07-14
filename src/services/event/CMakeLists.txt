set(CALIPER_EVENT_SOURCES
    EventTrigger.cpp)

add_service_sources(${CALIPER_EVENT_SOURCES})
add_caliper_service("event")
