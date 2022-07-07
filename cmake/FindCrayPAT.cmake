# Find CrayPAT libraries/headers

find_path(CRAYPAT_ROOT
  NAMES include/pat_api.h
)

find_library(CRAYPAT_LIBRARIES
  NAMES _pat_base
  HINTS ${CRAYPAT_ROOT}/lib
)

find_path(CRAYPAT_INCLUDE_DIRS
  NAMES pat_api.h
  HINTS ${CRAYPAT_ROOT}/include
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(CRAYPAT
  DEFAULT_MSG
  CRAYPAT_LIBRARIES
  CRAYPAT_INCLUDE_DIRS
)

mark_as_advanced(
  CRAYPAT_INCLUDE_DIRS
  CRAYPAT_LIBRARIES
)
