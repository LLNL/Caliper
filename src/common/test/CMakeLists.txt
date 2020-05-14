set(CALIPER_COMMON_TEST_SOURCES
  test_c_variant.cpp
  test_compressedsnapshotrecord.cpp
  test_runtimeconfig.cpp
  test_snapshotbuffer.cpp
  test_snapshottextformatter.cpp
  test_stringconverter.cpp
  test_variant.cpp)

add_executable(test_caliper-common
  $<TARGET_OBJECTS:caliper-common>
  ${CALIPER_COMMON_TEST_SOURCES})

target_link_libraries(test_caliper-common gtest_main)

add_custom_target(caliper-common_test.config ALL
  COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_CURRENT_SOURCE_DIR}/caliper-common_test.config
    ${CMAKE_CURRENT_BINARY_DIR}/caliper-common_test.config)

add_test(NAME test-caliper-common COMMAND test_caliper-common)
