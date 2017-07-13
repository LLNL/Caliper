# Build Sphinx documentation
# Adapted from 
# http://ericscottbarr.com/blog/2012/03/sphinx-and-cmake-beautiful-documentation-for-c-projects/

# Find Sphinx
find_package(Sphinx)

if(SPHINX_FOUND)
  if(NOT DEFINED SPHINX_THEME)
    set(SPHINX_THEME classic)
  endif()

  if(NOT DEFINED SPHINX_THEME_DIR)
    set(SPHINX_THEME_DIR)
  endif()

  set(BINARY_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/_build")

  set(SPHINX_CACHE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_doctrees")

  set(SPHINX_HTML_DIR "${CMAKE_CURRENT_BINARY_DIR}/html")

  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/conf.py.in"
    "${BINARY_BUILD_DIR}/conf.py" 
    @ONLY)

  add_custom_target(caliper_sphinx_docs ALL
    ${SPHINX_EXECUTABLE}
      -q -b html
      -c "${BINARY_BUILD_DIR}"
      -d "${SPHINX_CACHE_DIR}"
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${SPHINX_HTML_DIR}"
      COMMENT "Building Sphinx documentation")
endif()

find_package(Doxygen)

if (DOXYGEN_FOUND)
  set(DOXYFILE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
  set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

  configure_file(${DOXYFILE_SRC} ${DOXYFILE_OUT} @ONLY)

  add_custom_target(caliper_doxygen_doc ALL
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
    COMMENT "Building Doxygen documentation")
endif()
