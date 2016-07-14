set(CALIPER_CMAKE_SERVICE_SOURCES
        CaliperCMakeService.cpp)

add_service_sources(${CALIPER_CMAKE_SERVICE_SOURCES})
add_caliper_service("cmake")
