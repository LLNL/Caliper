include_directories ("../../common")
include_directories ("../../reader")
include_directories ("../util")
include_directories ("../../caliper")

set(CALIPER_QUERY_SOURCES
    cali-query.cpp)

add_executable(cali-query ${CALIPER_QUERY_SOURCES})

target_link_libraries(cali-query caliper-reader)
target_link_libraries(cali-query caliper-common)
target_link_libraries(cali-query caliper-tools-util)
target_link_libraries(cali-query caliper)

install(TARGETS cali-query DESTINATION ${CMAKE_INSTALL_BINDIR})
