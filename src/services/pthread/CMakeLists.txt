set(CALIPER_PTHREAD_SOURCES
    PthreadService.cpp)

# add_library(caliper-pthread ${CALIPER_PTHREAD_SOURCES})

# target_link_libraries(caliper-pthread caliper-common)
# target_link_libraries(caliper-pthread caliper)

# install(TARGETS caliper-pthread DESTINATION lib)

add_service_sources(${CALIPER_PTHREAD_SOURCES})
