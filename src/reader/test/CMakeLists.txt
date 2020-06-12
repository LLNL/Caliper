set(CALIPER_READER_TEST_SOURCES
  test_aggregator.cpp
  test_calqlparser.cpp
  test_filter.cpp
  test_flatexclusiveregionprofile.cpp
  test_flatinclusiveregionprofile.cpp
  test_metadb.cpp
  test_nestedexclusiveregionprofile.cpp
  test_nestedinclusiveregionprofile.cpp
  test_nodebuffer.cpp
  test_preprocessor.cpp)

add_executable(test_caliper-reader
  $<TARGET_OBJECTS:caliper-common>
  $<TARGET_OBJECTS:caliper-reader>
  ${CALIPER_READER_TEST_SOURCES})

target_link_libraries(test_caliper-reader gtest_main)

add_test(NAME test-caliper-reader COMMAND test_caliper-reader)
