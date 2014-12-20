set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS}  -Wall -std=c99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")

add_subdirectory(common)
add_subdirectory(caliper)
add_subdirectory(services)
add_subdirectory(tools)
