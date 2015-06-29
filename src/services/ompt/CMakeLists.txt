include_directories(${OMPT_INCLUDE_DIR})

set(CALIPER_OMPT_SOURCES
    OmptService.cpp)

# add_library(caliper-ompt ${CALIPER_OMPT_SOURCES})

# target_link_libraries(caliper-ompt caliper-common)
# target_link_libraries(caliper-ompt caliper)

# install(TARGETS caliper-ompt DESTINATION lib)

add_library(caliper-ompt OBJECT ${CALIPER_OMPT_SOURCES})

add_service_objlib("$<TARGET_OBJECTS:caliper-ompt>")
