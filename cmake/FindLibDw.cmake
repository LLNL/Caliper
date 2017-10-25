# Try to find LIBDW headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(ElfUtils)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  LIBDW_PREFIX         Set this variable to the root installation of
#                      libpapi if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  LIBDW_FOUND              System has LIBDW libraries and headers
#  LIBDW_LIBRARIES          The LIBDW library
#  LIBDW_INCLUDE_DIRS       The location of LIBDW headers

if (LIBDW_INCLUDE_DIR AND LIBDW_LIBRARY)
  set(LIBDW_FIND_QUIETLY true)
endif()

find_path(LIBDW_INCLUDE_DIR elfutils/libdw.h
  HINTS ${LIBDW_PREFIX}/include
)

find_library(LIBDW_LIBRARY 
  NAMES dw
  HINTS ${LIBDW_PREFIX}/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBDW DEFAULT_MSG
  LIBDW_LIBRARY
  LIBDW_INCLUDE_DIR
)

mark_as_advanced(
  LIBDW_INCLUDE_DIR
  LIBDW_LIBRARY
)
