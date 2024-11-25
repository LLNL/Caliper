set(CALIPER_EVENT_SOURCES
    AsyncEventTrigger.cpp
    EventTrigger.cpp)

add_service_sources(${CALIPER_EVENT_SOURCES})
add_caliper_service("event")
add_caliper_service("async_event")