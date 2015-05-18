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

find_path(PAPI_PREFIX
    NAMES include/papi.h
)

find_library(PAPI_LIBRARIES
    # Pick the static library first for easier run-time linking.
    NAMES libpapi.a papi
    HINTS ${PAPI_PREFIX}/lib ${HILTIDEPS}/lib
)

find_path(PAPI_INCLUDE_DIRS
    NAMES papi.h
    HINTS ${PAPI_PREFIX}/include ${HILTIDEPS}/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PAPI DEFAULT_MSG
    PAPI_LIBRARIES
    PAPI_INCLUDE_DIRS
)

mark_as_advanced(
    PAPI_PREFIX_DIRS
    PAPI_LIBRARIES
    PAPI_INCLUDE_DIRS
)
