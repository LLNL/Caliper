include_directories ("../src/common")
include_directories ("../src/caliper")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")

add_executable(cali-basic cali-basic.cpp)

target_link_libraries(cali-basic caliper)

