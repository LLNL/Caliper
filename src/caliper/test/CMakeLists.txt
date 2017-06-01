set(CALIPER_TEST_SOURCES
  test_compressedsnapshotrecord.cpp)

add_executable(test_caliper ${CALIPER_TEST_SOURCES})
target_link_libraries(test_caliper caliper gtest_main)

add_test(NAME test-caliper COMMAND test_caliper)
