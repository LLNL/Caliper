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

set(CALIPER_TEST_JSON_FILES
  test_single_config.json
  test_config_list.json
  test_config_and_options.json)

foreach(file ${CALIPER_TEST_JSON_FILES})
  add_custom_target(${file} ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink
      ${CMAKE_CURRENT_SOURCE_DIR}/${file}
      ${CMAKE_CURRENT_BINARY_DIR}/${file})
endforeach()

add_test(NAME test-caliper COMMAND test_caliper)
