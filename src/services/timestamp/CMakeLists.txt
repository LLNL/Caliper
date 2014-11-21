include_directories("../../common")
include_directories("../../caliper")

set(CALIPER_TIMESTAMP_SOURCES
    Timestamp.cpp)

add_library(caliper-timestamp ${CALIPER_TIMESTAMP_SOURCES})

target_link_libraries(caliper-timestamp caliper-common)
target_link_libraries(caliper-timestamp caliper)

install(TARGETS caliper-timestamp DESTINATION lib)
