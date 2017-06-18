set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=c99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")

add_executable(cali-basic cali-basic.cpp)
# add_executable(cali-wrap cali-wrap.cpp)
add_executable(cali-test cali-test.cpp)
add_executable(cali-basic-c cali-basic-c.c)
add_executable(cali-test-c cali-test-c.c)
add_executable(cali-print-snapshot cali-print-snapshot.c)

add_executable(cali-throughput-pthread cali-throughput-pthread.cpp)

add_executable(cali-simplereader-test cali-simplereader-test.cpp)

set_target_properties(cali-basic-c PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(cali-test-c PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(cali-print-snapshot PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(cali-test caliper)
target_link_libraries(cali-basic caliper)
target_link_libraries(cali-basic-c caliper)
target_link_libraries(cali-test-c caliper)
target_link_libraries(cali-print-snapshot caliper)
target_link_libraries(cali-throughput-pthread caliper caliper-tools-util Threads::Threads)
# target_link_libraries(cali-wrap caliper)

target_link_libraries(cali-simplereader-test caliper-reader)

if (BUILD_TESTING)
  add_subdirectory(ci_app_tests)
endif()

