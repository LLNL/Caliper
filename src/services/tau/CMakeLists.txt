set(CALIPER_TAU_SOURCES
    tau.cpp)

add_service_sources(${CALIPER_TAU_SOURCES})
add_caliper_service("tau CALIPER_HAVE_TAU")
