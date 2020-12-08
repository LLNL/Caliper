# Find ROCTracer libraries/headers

find_path(ROCM_PREFIX
  NAMES include/roctracer/roctx.h
)

find_library(ROCM_LIBRARIES
  NAMES roctx64
  HINTS ${ROCM_PREFIX}/lib
)

find_path(ROCM_INCLUDE_DIRS
  NAMES roctx.h
  HINTS ${ROCM_PREFIX}/include/roctracer
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(ROCM
  DEFAULT_MSG
  ROCM_LIBRARIES
  ROCM_INCLUDE_DIRS
)

mark_as_advanced(
  ROCM_INCLUDE_DIRS
  ROCM_LIBRARIES
)
