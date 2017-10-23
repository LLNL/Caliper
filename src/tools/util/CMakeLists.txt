set(CALIPER_TOOLS_UTIL_SOURCES
    Args.cpp)

add_library(caliper-tools-util ${CALIPER_TOOLS_UTIL_SOURCES})

set_target_properties(caliper-tools-util PROPERTIES SOVERSION ${CALIPER_MAJOR_VERSION})
set_target_properties(caliper-tools-util PROPERTIES VERSION ${CALIPER_VERSION})

install(TARGETS caliper-tools-util 
  EXPORT caliper
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
