# Find RocTX libraries/headers

find_path(ROCM_PATH
  NAMES include/roctracer/roctx.h
)

find_library(ROCTX_LIBRARIES
  NAMES roctx64
  HINTS ${ROCM_PATH}/lib
)

find_path(ROCTX_INCLUDE_DIRS
  NAMES roctx.h
  HINTS ${ROCM_PATH}/include/roctracer
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(ROCTX
  DEFAULT_MSG
  ROCTX_LIBRARIES
  ROCTX_INCLUDE_DIRS
)

mark_as_advanced(
  ROCTX_INCLUDE_DIRS
  ROCTX_LIBRARIES
)
