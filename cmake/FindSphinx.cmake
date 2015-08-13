# @file FindSphinx.cmake
#       Find the Sphinx documentation generator
#
# Adapted from 
# http://ericscottbarr.com/blog/2012/03/sphinx-and-cmake-beautiful-documentation-for-c-projects/
#
# If successful the following variables will be defined
# SPHINX_FOUND
# SPHINX_EXECUTABLE

find_program(SPHINX_EXECUTABLE 
  NAMES sphinx-build sphinx-build2
  HINTS
  $ENV{SPHINX_DIR}
  PATH_SUFFIXES bin
  DOC "Sphinx documentation generator")

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Sphinx DEFAULT_MSG SPHINX_EXECUTABLE)

mark_as_advanced(SPHINX_EXECUTABLE)
