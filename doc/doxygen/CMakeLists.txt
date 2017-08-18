set(DOXYFILE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

configure_file(${DOXYFILE_SRC} ${DOXYFILE_OUT} @ONLY)

add_custom_target(caliper_doxygen_doc ALL
  COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
  COMMENT "Building Doxygen documentation")
