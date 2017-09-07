set(CALIPER_READER_TEST_SOURCES
  test_aggregator.cpp
  test_filter.cpp
  test_nodebuffer.cpp)

add_executable(test_caliper-reader ${CALIPER_READER_TEST_SOURCES})
target_link_libraries(test_caliper-reader caliper-reader gtest_main)

add_test(NAME test-caliper-reader COMMAND test_caliper-reader)
