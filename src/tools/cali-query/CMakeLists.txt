include_directories ("../../common")
include_directories ("../../reader")

set(CALIPER_QUERY_SOURCES
    cali-query.cpp)

add_executable(cali-query ${CALIPER_QUERY_SOURCES})

target_link_libraries(cali-query caliper-reader)
target_link_libraries(cali-query caliper-common)

install(TARGETS cali-query DESTINATION bin)
