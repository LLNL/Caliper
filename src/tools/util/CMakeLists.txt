set(CALIPER_TOOLS_UTIL_SOURCES
    Args.cpp)
set(CALIPER_TOOLS_UTIL_HEADERS
    Args.h)

add_library(caliper-tools-util ${CALIPER_TOOLS_UTIL_SOURCES})

install(FILES ${CALIPER_TOOLS_UTIL_HEADERS} DESTINATION include)

install(TARGETS caliper-tools-util 
  EXPORT caliper
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)
