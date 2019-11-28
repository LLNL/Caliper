set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD   99)

set(CALIPER_TEST_APPS
  cali-annotation-perftest
  cali-flush-perftest
  cali-test)

find_package(OpenMP)

foreach(app ${CALIPER_TEST_APPS})
  add_executable(${app} ${app}.cpp)
  target_link_libraries(${app} caliper)
endforeach()

if (CALIPER_HAVE_ADIAK)
  target_include_directories(cali-annotation-perftest PRIVATE ${adiak_INCLUDE_DIRS})
  target_link_libraries(cali-annotation-perftest adiak)
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
