# Find CUPTI library/header

find_path(CUPTI_PREFIX
  NAMES include/cupti.h
)

find_library(CUPTI_LIBRARY 
  NAMES cupti
  HINTS ${CUPTI_PREFIX}/lib
)

find_path(CUPTI_INCLUDE_DIR 
  NAMES cupti.h
  HINTS ${CUPTI_PREFIX}/include
)

# we also need the CUDA include dir b/c cupti.h includes cuda.h
find_path(CUDA_INCLUDE_DIR
  NAMES cuda.h
  HINTS ${CUDA_PREFIX}/include
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(CUPTI 
  DEFAULT_MSG
  CUPTI_LIBRARY
  CUPTI_INCLUDE_DIR
  CUDA_INCLUDE_DIR
)

mark_as_advanced(
  CUPTI_INCLUDE_DIR
  CUPTI_LIBRARY
)
