set(CALIPER_COMMON_TEST_SOURCES
  test_variant.cpp)

add_executable(test_caliper-common ${CALIPER_COMMON_TEST_SOURCES})
target_link_libraries(test_caliper-common caliper-common gtest_main)

add_test(NAME test-caliper-common COMMAND test_caliper-common)
