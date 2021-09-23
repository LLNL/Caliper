# Find ROCTracer libraries/headers

find_path(ROCM_PREFIX
  NAMES include/roctracer/roctracer.h
)

find_library(ROCTRACER_LIBRARIES
  NAMES roctracer64
  HINTS ${ROCM_PREFIX}/lib
)

find_path(ROCTRACER_INCLUDE_DIR
  NAMES roctracer.h
  HINTS ${ROCM_PREFIX}/include/roctracer
)

find_path(HIP_INCLUDE_DIR
  NAMES hip/hip_runtime.h
  HINTS ${ROCM_PREFIX}/include)

set(ROCTRACER_INCLUDE_DIRS 
  ${ROCTRACER_INCLUDE_DIR}
  ${HIP_INCLUDE_DIR}
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(ROCTRACER
  DEFAULT_MSG
  ROCTRACER_LIBRARIES
  ROCTRACER_INCLUDE_DIRS
)

mark_as_advanced(
  ROCTRACER_INCLUDE_DIRS
  ROCTRACER_LIBRARIES
)
