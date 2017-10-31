# Try to find SOSFlow headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(ElfUtils)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  SOS_PREFIX         Set this variable to the root installation of
#                      libpapi if the module has problems finding the
#                      proper installation path.
#
# Variables defined by this module:
#
#  SOSFlow_FOUND              System has SOSFlow libraries and headers
#  SOSFlow_LIBRARIES          The SOSFlow library
#  SOSFlow_INCLUDE_DIRS       The location of SOSFlow headers

if (SOSFlow_INCLUDE_DIR AND SOSFlow_LIBRARY)
  set(SOSFlow_FIND_QUIETLY true)
endif()

find_path(SOSFlow_INCLUDE_DIR sos.h
  HINTS ${SOS_PREFIX}/include
)

find_library(SOSFlow_LIBRARY 
  NAMES sos
  HINTS ${SOS_PREFIX}/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SOSFlow DEFAULT_MSG
  SOSFlow_LIBRARY
  SOSFlow_INCLUDE_DIR
)

mark_as_advanced(
  SOSFlow_INCLUDE_DIR
  SOSFlow_LIBRARY
)
