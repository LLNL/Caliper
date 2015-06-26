include_directories(${OMPT_INCLUDE_DIR})

set(CALIPER_OMPT_SOURCES
    OmptService.cpp)

# add_library(caliper-ompt ${CALIPER_OMPT_SOURCES})

# target_link_libraries(caliper-ompt caliper-common)
# target_link_libraries(caliper-ompt caliper)

# install(TARGETS caliper-ompt DESTINATION lib)

add_service_sources(${CALIPER_OMPT_SOURCES})
