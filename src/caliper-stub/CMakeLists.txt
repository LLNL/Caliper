include_directories("../common")
include_directories ("../caliper")

set(CALIPER_STUB_CXX_SOURCES
    Annotation.cpp
    cali.c)
set(CALIPER_STUB_C_SOURCES
    cali.c)

add_library(caliper-stub-c ${CALIPER_STUB_C_SOURCES})
add_library(caliper-stub ${CALIPER_STUB_CXX_SOURCES})

install(TARGETS caliper-stub-c
  EXPORT caliper-stub-c
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS caliper-stub
  EXPORT caliper-stub
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
