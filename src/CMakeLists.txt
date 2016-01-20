if (${CMAKE_CXX_COMPILER_ID} MATCHES Intel)
  # CMake does not have proper compiler feature support for Intel compilers :-/
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
else()
  set(CMAKE_C_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_STANDARD_REQUIRED ON) 

  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_C_STANDARD   99)
endif()

include_directories(${PROJECT_BINARY_DIR})

add_subdirectory(common)
add_subdirectory(services)
add_subdirectory(caliper)
add_subdirectory(caliper-stub)
add_subdirectory(reader)
add_subdirectory(tools)

