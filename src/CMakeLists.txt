set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS}  -Wall -std=c99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")

include_directories(${PROJECT_BINARY_DIR})

add_subdirectory(common)
add_subdirectory(services)
add_subdirectory(caliper)
add_subdirectory(caliper-stub)
add_subdirectory(reader)
add_subdirectory(tools)

