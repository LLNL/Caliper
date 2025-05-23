include_directories("../../src/caliper")
include_directories("../../src/interface/c_fortran")

# build the C/C++ CI test apps

set(CALIPER_CI_CXX_TEST_APPS
  ci_test_basic
  ci_test_binding
  ci_test_io
  ci_test_macros
  ci_test_nesting
  ci_test_thread)
set(CALIPER_CI_C_TEST_APPS
  ci_test_alloc
  ci_test_alloc_hooks
  ci_test_c_ann
  ci_test_c_snapshot
  ci_test_channel_api)
set(CALIPER_CI_MPI_TEST_APPS
  ci_test_cali_before_mpi
  ci_test_collective_output_channel
  ci_test_mpi_before_cali
  ci_test_mpi_channel_manager)
set(CALIPER_CI_Fortran_TEST_APPS
  ci_test_f_ann)
set(CALIPER_CI_Python_TEST_APPS
  ci_test_py_ann.py
)

foreach(app ${CALIPER_CI_CXX_TEST_APPS})
  add_executable(${app} ${app}.cpp)
  target_link_libraries(${app} caliper)
endforeach()

target_link_libraries(ci_test_thread  Threads::Threads)
target_link_libraries(ci_test_nesting Threads::Threads)

foreach(app ${CALIPER_CI_C_TEST_APPS})
  add_executable(${app} ${app}.c)
  set_target_properties(${app} PROPERTIES LINKER_LANGUAGE CXX)
  target_compile_features(${app} PRIVATE c_std_99)
  target_link_libraries(${app} caliper)
endforeach()

# add symlinks to the python test driver scripts
set(PYTHON_SCRIPTS
  test_allocservice.py
  test_basictrace.py
  test_c_api.py
  test_caliquery.py
  test_file_io.py
  test_json.py
  test_log.py
  test_loopreport.py
  test_monitor.py
  test_report.py
  test_runtimereport.py
  test_spot.py
  test_templateservices.py
  test_textlog.py
  test_thread.py
  test_validator.py
  calipertest.py)

if (CALIPER_HAVE_CPUINFO)
  list(APPEND PYTHON_SCRIPTS test_cpuinfo.py)
endif()
if (CALIPER_HAVE_MEMUSAGE)
  list(APPEND PYTHON_SCRIPTS test_memusageservice.py)
endif()
if (CALIPER_HAVE_LIBUNWIND)
  list(APPEND PYTHON_SCRIPTS test_callpath.py)
endif()
if (CALIPER_HAVE_GOTCHA)
  list(APPEND PYTHON_SCRIPTS
    test_alloc_hooks.py
    test_ioservice.py
    test_pthread.py)
endif()
if (CALIPER_HAVE_PAPI)
  list(APPEND PYTHON_SCRIPTS test_papi.py)
endif()
if (CALIPER_HAVE_LIBPFM)
  list(APPEND PYTHON_SCRIPTS test_libpfm.py)
endif()
if (CALIPER_HAVE_SAMPLER)
  list(APPEND PYTHON_SCRIPTS test_sampler.py)
  if (CALIPER_HAVE_LIBDW)
    list(APPEND PYTHON_SCRIPTS test_symbollookup.py)
    if (CALIPER_HAVE_LIBUNWIND)
      list(APPEND PYTHON_SCRIPTS test_samplereport.py)
    endif()
  endif()
endif()

if (CALIPER_HAVE_OMPT AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.9)
  find_package(OpenMP REQUIRED)
  add_executable(ci_test_openmp ci_test_openmp.cpp)
  target_link_libraries(ci_test_openmp caliper OpenMP::OpenMP_CXX)

  list(APPEND PYTHON_SCRIPTS
    test_ompt.py)
endif()

if (CALIPER_HAVE_MPI)
  foreach(app ${CALIPER_CI_MPI_TEST_APPS})
    add_executable(${app} ${app}.cpp)
    set(calitest_mpi_include_dirs
      ${MPI_CXX_INCLUDE_PATH}
      ${CMAKE_CURRENT_SOURCE_DIR}/../../mpi/include)
    target_include_directories(${app} PRIVATE ${calitest_mpi_include_dirs})
    target_link_libraries(${app} caliper ${MPI_CXX_LIBRARIES})
  endforeach()

  if (RUN_MPI_TESTS)
    list(APPEND PYTHON_SCRIPTS test_mpi.py)

    if (CALIPER_HAVE_TAU)
      list(APPEND PYTHON_SCRIPTS test_tau_service.py)
    endif()
  endif()
endif()

if (WITH_FORTRAN)
  foreach(app ${CALIPER_CI_Fortran_TEST_APPS})
    add_executable(${app} ${app}.f)
    set_source_files_properties(${app}.f PROPERTIES Fortran_FORMAT FREE)
    target_link_libraries(${app} caliper)
  endforeach()

  list(APPEND PYTHON_SCRIPTS test_fortran_api.py)
endif()

if (WITH_PYTHON_BINDINGS)
  foreach(file ${CALIPER_CI_Python_TEST_APPS})
    # add_custom_target(${file} ALL
    #   COMMAND ${CMAKE_COMMAND} -E create_symlink
    #     ${CMAKE_CURRENT_SOURCE_DIR}/${file}
    #     ${CMAKE_CURRENT_BINARY_DIR}/${file})
    configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/${file}
      ${CMAKE_CURRENT_BINARY_DIR}/${file}
      @ONLY
    )
  endforeach()

  list(APPEND PYTHON_SCRIPTS test_python_api.py)
endif()

set(DATA_FILES
  example_node_info.json)

foreach(file ${PYTHON_SCRIPTS} ${DATA_FILES})
  add_custom_target(${file} ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink
      ${CMAKE_CURRENT_SOURCE_DIR}/${file}
      ${CMAKE_CURRENT_BINARY_DIR}/${file})
endforeach()

add_test(NAME CI_app_tests COMMAND ${CALI_PYTHON_EXECUTABLE} -B -m unittest discover -p "test_*.py")
# if (WITH_PYTHON_BINDINGS)
#   message(STATUS "Adding ${PYPATH_TESTING} to PYTHONPATH for CI_app_tests")
#   set_tests_properties(CI_app_tests PROPERTIES ENVIRONMENT "PYTHONPATH=${PYPATH_TESTING}")
# endif()
