project(caliper-examples)

include_directories("../../src/interface/c_fortran")

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD   99)

set(CALIPER_CXX_EXAMPLE_APPS
  cxx-example
  cali-memtracking
  cali-memtracking-macros
  cali-perfproblem-branch-mispred
  cali-functional
  cali-regionprofile)
set(CALIPER_C_EXAMPLE_APPS
  c-example
  cali-print-snapshot)
set(CALIPER_Fortran_EXAMPLE_APPS
  fortran-example
  fortran-regionprofile)

foreach(app ${CALIPER_CXX_EXAMPLE_APPS})
  add_executable(${app} ${app}.cpp)
  target_link_libraries(${app} caliper)
endforeach()

foreach(app ${CALIPER_C_EXAMPLE_APPS})
  add_executable(${app} ${app}.c)
  target_link_libraries(${app} caliper)
  set_target_properties(${app} PROPERTIES LINKER_LANGUAGE CXX)
endforeach()

if (WITH_FORTRAN)
  foreach(app ${CALIPER_Fortran_EXAMPLE_APPS})
    add_executable(${app} ${app}.f)
    set_source_files_properties(${app}.f PROPERTIES Fortran_FORMAT FREE)
    target_include_directories(${app} PRIVATE ${CMAKE_Fortran_MODULE_DIRECTORY})
    target_link_libraries(${app} caliper)
  endforeach()
endif()

target_link_libraries(cali-memtracking
  caliper-tools-util)
target_link_libraries(cali-memtracking-macros
  caliper-tools-util)
