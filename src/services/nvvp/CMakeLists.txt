set(CALIPER_NVVP_SOURCES
    nvvp.cpp)

add_service_sources(${CALIPER_NVVP_SOURCES})
add_caliper_service("nvvp CALIPER_HAVE_NVVP")
