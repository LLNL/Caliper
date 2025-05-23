project(caliper-examples)

include_directories("../../src/interface/c_fortran")

set(CALIPER_CXX_EXAMPLE_APPS
  async_event
  cxx-example
  cali-memtracking
  cali-memtracking-macros
  cali-perfproblem-branch-mispred
  cali-regionprofile)
set(CALIPER_MPI_EXAMPLE_APPS
  collective-output-channel)
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
  target_compile_features(${app} PRIVATE c_std_99)
  set_target_properties(${app} PROPERTIES LINKER_LANGUAGE CXX)
endforeach()

if (CALIPER_HAVE_MPI)
  foreach(app ${CALIPER_MPI_EXAMPLE_APPS})
    add_executable(${app} ${app}.cpp)
    set(examples_mpi_include_dirs
      ${MPI_CXX_INCLUDE_PATH}
      ${CMAKE_CURRENT_SOURCE_DIR}/../../mpi/include)
    target_include_directories(${app} PRIVATE ${examples_mpi_include_dirs})
    target_link_libraries(${app} caliper ${MPI_CXX_LIBRARIES})
  endforeach()
endif()

if (WITH_FORTRAN)
  foreach(app ${CALIPER_Fortran_EXAMPLE_APPS})
    add_executable(${app} ${app}.f)
    set_source_files_properties(${app}.f PROPERTIES Fortran_FORMAT FREE)
    target_link_libraries(${app} caliper)
  endforeach()
endif()

target_link_libraries(cali-memtracking
  caliper-tools-util)
target_link_libraries(cali-memtracking-macros
  caliper-tools-util)
target_link_libraries(async_event
  Threads::Threads)