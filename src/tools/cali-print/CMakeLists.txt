include_directories ("../../common")
include_directories ("../../reader")
include_directories ("../util")
include_directories ("../../caliper")

set(CALIPER_PRINT_SOURCES
    cali-print.cpp)

add_executable(cali-print ${CALIPER_PRINT_SOURCES})

target_link_libraries(cali-print caliper-reader)
target_link_libraries(cali-print caliper-common)
target_link_libraries(cali-print caliper-tools-util)
target_link_libraries(cali-print caliper)

install(TARGETS cali-print DESTINATION bin)
