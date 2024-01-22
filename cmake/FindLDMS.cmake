# Try to find LDMS headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(LDMS)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  LDMS_PREFIX         Set this variable to the root installation of
#                      LDMS if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  LDMS_FOUND              System has LDMS libraries and headers
#  LDMS_LIBRARIES          The LDMS library
#  LDMS_INCLUDE_DIRS       The location of LDMS headers

find_library(LDMS_LIBLDMS
    NAMES ldms
    PATHS /usr/lib64 /usr/lib /opt/ovis/lib
)

find_library(LDMS_LIBSTREAM
    NAMES ldmsd_stream
    PATHS /usr/lib64 /usr/lib /opt/ovis/lib
)

find_path(LDMS_INCLUDE_DIRS
    NAMES "ldms.h" "ldmsd_stream.h"
    PATH_SUFFIXES "ldms"
)
message(STATUS "libldms.so => ${LDMS_LIBRARIES}")
message(STATUS "ldmsd_stream.h => ${LDMS_INCLUDE_DIRS}")
message(STATUS "ldms.h => ${LDMS_INCLUDE_DIRS}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LDMS DEFAULT_MSG
    LDMS_LIBLDMS
    LDMS_LIBSTREAM
    LDMS_INCLUDE_DIRS
)

mark_as_advanced(
    LDMS_PREFIX_DIRS
    LDMS_LIBLDMS
    LDMS_LIBSTREAM
    LDMS_INCLUDE_DIRS
)
