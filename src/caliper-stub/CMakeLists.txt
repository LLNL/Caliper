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
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)
install(TARGETS caliper-stub
  EXPORT caliper-stub
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)
