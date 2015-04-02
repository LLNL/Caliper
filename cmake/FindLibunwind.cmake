#
# - Find libunwind
#
# LIBUNWIND_INCLUDE_DIR - Path to libunwind.h
# LIBUNWIND_LIBRARY     - List of libraries for using libunwind
# LIBUNWIND_FOUND       - True if libunwind was found

if(LIBUNWIND_INCLUDE_DIR)
  set(LIBUNWIND_FIND_QUIETLY true)
endif()

find_path(LIBUNWIND_INCLUDE_DIR libunwind.h)
find_library(LIBUNWIND_LIBRARY unwind)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBUNWIND DEFAULT_MSG LIBUNWIND_LIBRARY LIBUNWIND_INCLUDE_DIR)

mark_as_advanced(LIBUNWIND_LIBRARY LIBUNWIND_INCLUDE_DIR)
