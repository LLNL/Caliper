set(CALIPER_VALIDATOR_SOURCES
    validator.cpp)

add_service_sources(${CALIPER_VALIDATOR_SOURCES})
add_caliper_service("validator")
