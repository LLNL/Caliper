include_directories("../common")
include_directories ("../caliper")

set(CALIPER_STUB_SOURCES
    Annotation.cpp
    cali.cpp)

add_library(caliper-stub ${CALIPER_STUB_SOURCES})

install(TARGETS caliper-stub
  EXPORT caliper-stub 
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)
