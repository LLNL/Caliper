include_directories(${PAPI_INCLUDE_DIRS})

set(CALIPER_PAPI_SOURCES
    Papi.cpp)

# add_library(caliper-papi ${CALIPER_PAPI_SOURCES})

# target_link_libraries(caliper-papi caliper-common)
# target_link_libraries(caliper-papi caliper)
# target_link_libraries(caliper-papi ${PAPI_LIBRARIES})

# install(TARGETS caliper-papi DESTINATION lib)

add_service_sources(${CALIPER_PAPI_SOURCES})

# target_link_libraries(caliper ${PAPI_LIBRARIES})
