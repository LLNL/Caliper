set(CALIPER_TOOLS_UTIL_SOURCES
    Args.cpp)

add_library(caliper-tools-util ${CALIPER_TOOLS_UTIL_SOURCES})

target_compile_features(caliper-tools-util PUBLIC cxx_std_11)

set_target_properties(caliper-tools-util PROPERTIES SOVERSION ${CALIPER_MAJOR_VERSION})
set_target_properties(caliper-tools-util PROPERTIES VERSION ${CALIPER_VERSION})

install(TARGETS caliper-tools-util 
  EXPORT caliper-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
