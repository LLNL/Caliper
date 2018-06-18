include_directories(${gotcha_INCLUDE_DIRS})

set(CALIPER_SYSALLOC_SOURCES
  SysAllocService.cpp)

add_library(caliper-sysalloc OBJECT ${CALIPER_SYSALLOC_SOURCES})

add_service_objlib("caliper-sysalloc")
add_caliper_service("sysalloc CALIPER_HAVE_GOTCHA")
