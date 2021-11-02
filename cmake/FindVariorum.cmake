# Try to find Variorum headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(Variorum)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  VARIORUM_PREFIX     Set this variable to the root installation of
#                      libvariorum if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  VARIORUM_FOUND              System has Variorum libraries and headers
#  VARIORUM_LIBRARIES          The Variorum PAPI library
#  VARIORUM_INCLUDE_DIRS       The location of Variorum header

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
