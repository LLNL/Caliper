# Find ROCTracer libraries/headers

find_path(ROCM_ROOT_DIR
  NAMES include/roctracer/roctracer.h
)

find_library(ROCTRACER_LIBROCTRACER
  NAMES roctracer64
  HINTS ${ROCM_ROOT_DIR}/lib
)
find_library(ROCTRACER_LIBHSARUNTIME
  NAMES hsa-runtime64
  HINTS ${ROCM_ROOT_DIR}/lib
)
find_library(ROCTRACER_LIBHSAKMT
  NAMES hsakmt
  HINTS ${ROCM_ROOT_DIR}/lib
)

find_path(ROCTRACER_INCLUDE_DIR
  NAMES roctracer.h
  HINTS ${ROCM_ROOT_DIR}/include/roctracer
)

find_path(HIP_INCLUDE_DIR
  NAMES hip/hip_runtime.h
  HINTS ${ROCM_ROOT_DIR}/include)

set(ROCTRACER_INCLUDE_DIRS 
  ${ROCTRACER_INCLUDE_DIR}
  ${HIP_INCLUDE_DIR}
)
set(ROCTRACER_LIBRARIES
  ${ROCTRACER_LIBROCTRACER}
  ${ROCTRACER_LIBHSARUNTIME}
  ${ROCTRACER_LIBHSAKMT})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(ROCTRACER
  DEFAULT_MSG
  ROCTRACER_LIBROCTRACER
  ROCTRACER_LIBHSARUNTIME
  ROCTRACER_LIBHSAKMT
  ROCTRACER_INCLUDE_DIRS
)

mark_as_advanced(
  ROCTRACER_INCLUDE_DIRS
  ROCTRACER_LIBRARIES
)
