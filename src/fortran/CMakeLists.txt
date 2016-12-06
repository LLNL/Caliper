enable_language(Fortran)

add_executable(testf03 main.f90 caliper.f90)

set_target_properties(testf03 PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(testf03 caliper)

install(FILES caliper.f90 DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/fortran)
