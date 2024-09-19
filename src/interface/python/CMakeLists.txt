set(PYCALIPER_BINDING_SOURCES
    annotation.cpp
    config_manager.cpp
    instrumentation.cpp
    loop.cpp
    mod.cpp
)

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

find_package(pybind11 CONFIG REQUIRED)

set(PYCALIPER_SYSCONFIG_SCHEME "posix_user" CACHE STRING "Scheme used for searching for pycaliper's install path. Valid options can be determined with 'sysconfig.get_scheme_names()'")

execute_process(COMMAND ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/cmake/get_python_install_paths.py purelib ${PYCALIPER_SYSCONFIG_SCHEME} OUTPUT_VARIABLE PYCALIPER_SITELIB)
execute_process(COMMAND ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/cmake/get_python_install_paths.py platlib ${PYCALIPER_SYSCONFIG_SCHEME} OUTPUT_VARIABLE PYCALIPER_SITEARCH)

message(STATUS "Pycaliper sitelib: ${PYCALIPER_SITELIB}")
message(STATUS "Pycaliper sitearch: ${PYCALIPER_SITEARCH}")

set(PYCALIPER_SITELIB "${PYCALIPER_SITELIB}/pycaliper")
set(PYCALIPER_SITEARCH "${PYCALIPER_SITEARCH}/pycaliper")

pybind11_add_module(__pycaliper_impl ${PYCALIPER_BINDING_SOURCES})
target_link_libraries(__pycaliper_impl PUBLIC caliper)
target_compile_features(__pycaliper_impl PUBLIC cxx_std_11)
target_include_directories(__pycaliper_impl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_target(
    pycaliper_test ALL # Always build pycaliper_test
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/pycaliper
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/pycaliper ${CMAKE_CURRENT_BINARY_DIR}/pycaliper
    COMMENT "Copying pycaliper Python source to ${CMAKE_CURRENT_BINARY_DIR}/pycaliper"
)
add_dependencies(__pycaliper_impl pycaliper_test)

if (BUILD_TESTING)
    set(PYPATH_TESTING ${CMAKE_CURRENT_BINARY_DIR} CACHE INTERNAL "")
    add_custom_target(
        pycaliper_symlink_lib_in_build ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink
          $<TARGET_FILE:__pycaliper_impl>
          ${CMAKE_CURRENT_BINARY_DIR}/pycaliper/$<TARGET_FILE_NAME:__pycaliper_impl>
        COMMENT "Creating symlink between Python C module and build directory for testing"
        DEPENDS __pycaliper_impl
    )
    message(STATUS "Will add ${PYPATH_TESTING} to PYTHONPATH during test")
endif()

install(
    DIRECTORY
        pycaliper/
    DESTINATION
        ${PYCALIPER_SITELIB}
)

install(
    TARGETS
        __pycaliper_impl
    ARCHIVE DESTINATION
        ${PYCALIPER_SITEARCH}
    LIBRARY DESTINATION
        ${PYCALIPER_SITEARCH}
)