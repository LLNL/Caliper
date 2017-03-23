#
# --- googletest: download googletest at configure time
#
 
configure_file(googletest-download_CMakeLists.txt.in googletest-download/CMakeLists.txt)

execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE   result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)

if (result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE   result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)

if (result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding our compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directories. Defines gtest and gtest-main targets.
add_subdirectory(
  ${CMAKE_CURRENT_BINARY_DIR}/googletest-download/googletest-src
  ${CMAKE_CURRENT_BINARY_DIR}/googletest-download/googletest-build)
