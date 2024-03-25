set(CALIPER_TEST_APPS
  cali-annotation-perftest
  cali-flush-perftest
  cali-test)

find_package(OpenMP)

execute_process(
  COMMAND
    date "+%F %T"
  OUTPUT_VARIABLE
    CALI_BUILD_TIME
  OUTPUT_STRIP_TRAILING_WHITESPACE)

configure_file(build_metadata.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/build_metadata.cpp)

foreach(app ${CALIPER_TEST_APPS})
  add_executable(${app}
    ${app}.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/build_metadata.cpp)
  target_link_libraries(${app} caliper)
endforeach()

if (CALIPER_HAVE_ADIAK)
  target_include_directories(cali-annotation-perftest PRIVATE ${adiak_INCLUDE_DIRS})
  target_link_libraries(cali-annotation-perftest ${adiak_LIBRARIES})
endif()

set(CALIPER_TEST_OMP_APPS
  cali-annotation-perftest
  cali-flush-perftest)

if (${OpenMP_FOUND})
  foreach (app ${CALIPER_TEST_OMP_APPS})
    target_compile_options(${app}
      PRIVATE ${OpenMP_CXX_FLAGS})
    target_link_libraries(${app}
      ${OpenMP_CXX_FLAGS})
  endforeach()
endif()

target_link_libraries(cali-annotation-perftest
  caliper-tools-util)
target_link_libraries(cali-flush-perftest
  caliper-tools-util)

add_subdirectory(ci_app_tests)
