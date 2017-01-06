include_directories ("../../common")
include_directories ("../../reader")
include_directories ("../util")
include_directories ("../../caliper")

set(CALIPER_GRAPH_SOURCES
    cali-graph.cpp)

add_executable(cali-graph ${CALIPER_GRAPH_SOURCES})

target_link_libraries(cali-graph caliper-reader)
target_link_libraries(cali-graph caliper-common)
target_link_libraries(cali-graph caliper-tools-util)
target_link_libraries(cali-graph caliper)

install(TARGETS cali-graph DESTINATION ${CMAKE_INSTALL_BINDIR})
