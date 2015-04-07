include_directories ("../src/common")
include_directories ("../src/caliper")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=c99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")

add_executable(cali-basic cali-basic.cpp)
add_executable(cali-test cali-test.cpp)
add_executable(cali-basic-c cali-basic-c.c)

target_link_libraries(cali-test caliper)
target_link_libraries(cali-basic caliper)
target_link_libraries(cali-basic-c caliper)
