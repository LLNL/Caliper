set(CALIPER_SCOREP_SOURCES
    scorep.cpp)

add_service_sources(${CALIPER_SCOREP_SOURCES})
add_caliper_service("scorep CALIPER_HAVE_SCOREP")
