# Find Intel VTune Task API

find_path(ITT_INCLUDE_DIR
  NAMES ittnotify.h
  HINTS ${ITT_PREFIX}/include)

find_library(ITT_LIBRARY
  NAMES ittnotify
  HINTS ${ITT_PREFIX}/lib)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(ITT
  DEFAULT_MSG
  ITT_LIBRARY
  ITT_INCLUDE_DIR)