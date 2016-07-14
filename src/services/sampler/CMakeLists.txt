set(CALIPER_SAMPLER_SOURCES
    Sampler.cpp)

add_service_sources(${CALIPER_SAMPLER_SOURCES})
add_caliper_service("sampler CALIPER_HAVE_SAMPLER")
