#
# --- gotcha: download gotcha at configure time
#
 
configure_file(sos-download_CMakeLists.txt.in sos-download/CMakeLists.txt)

execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE   result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/sos-download)

if (result)
  message(FATAL_ERROR "CMake step for sos failed: ${result}")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE   result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/sos-download)

if (result)
  message(FATAL_ERROR "Build step for sos failed: ${result}")
endif()
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_BINARY_DIR}/sos-download/sos-src/cmake")
MESSAGE("Searching ${CMAKE_MODULE_PATH}")
include(${CMAKE_CURRENT_BINARY_DIR}/sos-download/sos-src/cmake/GetGitRevisionDescription.cmake)
# Add gotcha directories. Defines gotcha targets.
add_subdirectory(
  ${CMAKE_CURRENT_BINARY_DIR}/sos-download/sos-src
  ${CMAKE_CURRENT_BINARY_DIR}/sos-download/sos-build)
