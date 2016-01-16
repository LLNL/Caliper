set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD   99)

include_directories(${PROJECT_BINARY_DIR})

add_subdirectory(common)
add_subdirectory(services)
add_subdirectory(caliper)
add_subdirectory(caliper-stub)
add_subdirectory(reader)
add_subdirectory(tools)

