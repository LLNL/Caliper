# Try to find PAPI headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(PAPI)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  PAPI_PREFIX         Set this variable to the root installation of
#                      libpapi if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  PAPI_FOUND              System has PAPI libraries and headers
#  PAPI_LIBRARIES          The PAPI library
#  PAPI_INCLUDE_DIRS       The location of PAPI headers

find_path(VARIORUM_PREFIX
    NAMES include/variorum.h
)

find_library(VARIORUM_LIBRARIES
    NAMES variorum
    HINTS ${VARIORUM_PREFIX}/lib
)

find_path(VARIORUM_INCLUDE_DIRS
    NAMES variorum.h
    HINTS ${VARIORUM_PREFIX}/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VARIORUM DEFAULT_MSG
    VARIORUM_LIBRARIES
    VARIORUM_INCLUDE_DIRS
)

mark_as_advanced(
    VARIORUM_PREFIX_DIRS
    VARIORUM_LIBRARIES
    VARIORUM_INCLUDE_DIRS
)
