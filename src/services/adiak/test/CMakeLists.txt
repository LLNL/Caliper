set(CALIPER_ADIAK_SERVICES_TEST_SOURCES
  test_adiak.cpp)

add_executable(test_adiak_services ${CALIPER_ADIAK_SERVICES_TEST_SOURCES})
target_link_libraries(test_adiak_services caliper-serial adiak gtest_main)

add_test(NAME test-adiak-services COMMAND test_adiak_services)
