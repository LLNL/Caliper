set(CALIPER_GIT_SOURCES
    GitInfo.cpp)

add_service_sources(${CALIPER_GIT_SOURCES})
add_caliper_service("git")
