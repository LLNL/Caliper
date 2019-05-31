include_directories("../include")
include_directories("../../mpi/include")
include_directories(${MPI_CXX_INCLUDE_PATH})

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

set(CONFIG_MANAGER_SOURCES
  ConfigManager.cpp
  RuntimeProfileController.cpp
  SpotController.cpp)

add_library(caliper-config-manager
  ${CONFIG_MANAGER_SOURCES})

target_link_libraries(caliper-config-manager PUBLIC caliper)

if (CALIPER_HAVE_MPI)
  target_link_libraries(caliper-config-manager PUBLIC caliper-mpi)
endif()
  
if (BUILD_TESTING)
  add_subdirectory(test)
endif()
