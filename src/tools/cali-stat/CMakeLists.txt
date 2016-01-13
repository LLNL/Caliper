include_directories ("../../common")
include_directories ("../../reader")
include_directories ("../util")
include_directories ("../../caliper")

set(CALIPER_STAT_SOURCES
    cali-stat.cpp)

add_executable(cali-stat ${CALIPER_STAT_SOURCES})

target_link_libraries(cali-stat caliper-reader)
target_link_libraries(cali-stat caliper-common)
target_link_libraries(cali-stat caliper-tools-util)
target_link_libraries(cali-stat caliper)

install(TARGETS cali-stat DESTINATION bin)
