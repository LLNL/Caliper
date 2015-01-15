include_directories("../../common")
include_directories("../../caliper")

set(CALIPER_DEBUG_SOURCES
    Debug.cpp)

add_library(caliper-debug ${CALIPER_DEBUG_SOURCES})

target_link_libraries(caliper-debug caliper-common)
target_link_libraries(caliper-debug caliper)

install(TARGETS caliper-debug DESTINATION lib)
