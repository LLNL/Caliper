include_directories ("../src/common")
include_directories ("../src/caliper")
include_directories ("../src/tools/util")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=c99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")

add_executable(cali-basic cali-basic.cpp)
add_executable(cali-test cali-test.cpp)
add_executable(cali-basic-c cali-basic-c.c)
add_executable(cali-throughput-pthread cali-throughput-pthread.cpp)

set_target_properties(cali-basic-c PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(cali-test caliper)
target_link_libraries(cali-basic caliper)
target_link_libraries(cali-basic-c caliper)
target_link_libraries(cali-throughput-pthread caliper)
target_link_libraries(cali-throughput-pthread caliper-tools-util)
