set(CALIPER_TIMER_SOURCES
    Timer.cpp)

add_service_sources(${CALIPER_TIMER_SOURCES})

add_caliper_service("timer")
add_caliper_service("timestamp")