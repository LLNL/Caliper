set(CALIPER_ENV_SOURCES
    EnvironmentInfo.cpp)

add_service_sources(${CALIPER_ENV_SOURCES})
add_caliper_service("env")
