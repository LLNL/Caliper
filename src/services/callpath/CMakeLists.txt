include_directories(${LIBUNWIND_INCLUDE_DIRS})
if (CALIPER_HAVE_LIBDW)
  include_directories(${LIBDW_INCLUDE_DIR})
endif()

set(CALIPER_CALLPATH_SOURCES
    Callpath.cpp)

add_library(caliper-callpath OBJECT ${CALIPER_CALLPATH_SOURCES})

add_service_objlib("caliper-callpath")
add_caliper_service("callpath CALIPER_HAVE_LIBUNWIND")
