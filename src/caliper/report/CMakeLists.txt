set(CALIPER_REPORTERS_SOURCES
    TableReport.cpp)

add_library(caliper-reporters OBJECT ${CALIPER_REPORTERS_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-reporters PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()

