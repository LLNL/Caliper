set(CALIPER_MANAGER_TEST_SOURCES
  test_configmanager.cpp)

add_executable(test_caliper-manager
  ${CALIPER_MANAGER_TEST_SOURCES})

target_link_libraries(test_caliper-manager caliper-config-manager gtest_main)

add_test(NAME test-caliper-manager COMMAND test_caliper-manager)
