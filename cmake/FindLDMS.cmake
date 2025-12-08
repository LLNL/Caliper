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

# Set up search paths based on LDMS_PREFIX
set(LDMS_SEARCH_PATHS /usr/lib64 /usr/lib /opt/ovis/lib)
set(LDMS_INCLUDE_SEARCH_PATHS /usr/include /usr/local/include /opt/ovis/include)

if(LDMS_PREFIX)
    list(INSERT LDMS_SEARCH_PATHS 0 "${LDMS_PREFIX}/lib")
    list(INSERT LDMS_INCLUDE_SEARCH_PATHS 0 "${LDMS_PREFIX}/include")
endif()

find_library(LDMS_LIBLDMS
    NAMES ldms
    PATHS ${LDMS_SEARCH_PATHS}
)

find_library(LDMS_LIBSTREAM
    NAMES ldmsd_stream
    PATHS ${LDMS_SEARCH_PATHS}
)

find_path(LDMS_INCLUDE_DIRS
    NAMES "ldms/ldms.h" "ldms/ldmsd_stream.h"
    PATHS ${LDMS_INCLUDE_SEARCH_PATHS}
)

message(STATUS "libldms.so => ${LDMS_LIBLDMS}")
message(STATUS "libldmsd_stream.so => ${LDMS_LIBSTREAM}")
message(STATUS "LDMS include dir => ${LDMS_INCLUDE_DIRS}")

# Set LDMS_LIBRARIES to include both libraries
if(LDMS_LIBLDMS AND LDMS_LIBSTREAM)
    set(LDMS_LIBRARIES ${LDMS_LIBLDMS} ${LDMS_LIBSTREAM})
endif()

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
