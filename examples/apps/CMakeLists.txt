project(caliper-examples)

if (${CMAKE_CXX_COMPILER_ID} MATCHES Intel)
  # CMake does not have proper compiler feature support for Intel compilers :-/
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
else()
  set(CMAKE_C_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_STANDARD_REQUIRED ON) 

  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_C_STANDARD   99)
endif()

set(CALIPER_CXX_EXAMPLE_APPS
  cxx-example
  cali-memtracking
  cali-memtracking-macros
  cali-perfproblem-branch-mispred
  cali-functional
  cali-regionprofile)
set(CALIPER_C_EXAMPLE_APPS
  cali-basic-annotations-c
  cali-print-snapshot)

foreach(app ${CALIPER_CXX_EXAMPLE_APPS})
  add_executable(${app} ${app}.cpp)
  target_link_libraries(${app} caliper)
endforeach()

foreach(app ${CALIPER_C_EXAMPLE_APPS})
  add_executable(${app} ${app}.c)
  target_link_libraries(${app} caliper)
  set_target_properties(${app} PROPERTIES LINKER_LANGUAGE CXX)
endforeach()

target_link_libraries(cali-memtracking
  caliper-tools-util)
target_link_libraries(cali-memtracking-macros
  caliper-tools-util)
