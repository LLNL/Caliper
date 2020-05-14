set(CALIPER_TEST_SOURCES
  test_attribute.cpp
  test_blackboard.cpp
  test_c_api.cpp
  test_c_wrapper.cpp
  test_channel_api.cpp
  test_channel_controller.cpp
  test_configmanager.cpp
  test_metadatatree.cpp
  test_c_snapshot.cpp)

add_executable(test_caliper ${CALIPER_TEST_SOURCES})

target_link_libraries(test_caliper caliper-serial gtest_main)

add_test(NAME test-caliper COMMAND test_caliper)
