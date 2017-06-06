#
# --- gotcha: download gotcha at configure time
#
 
configure_file(gotcha-download_CMakeLists.txt.in gotcha-download/CMakeLists.txt)

execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE   result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gotcha-download)

if (result)
  message(FATAL_ERROR "CMake step for gotcha failed: ${result}")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE   result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gotcha-download)

if (result)
  message(FATAL_ERROR "Build step for gotcha failed: ${result}")
endif()

# Add gotcha directories. Defines gtest and gtest-main targets.
add_subdirectory(
  ${CMAKE_CURRENT_BINARY_DIR}/gotcha-download/gotcha-src
  ${CMAKE_CURRENT_BINARY_DIR}/gotcha-download/gotcha-build)
