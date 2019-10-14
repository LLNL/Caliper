set(CALIPER_CONTROLLERS_SOURCES
  EventTraceController.cpp
  HatchetRegionProfileController.cpp
  NvProfController.cpp
  RuntimeReportController.cpp
  controllers.cpp)

if (CALIPER_HAVE_SAMPLER)
  list(APPEND CALIPER_CONTROLLERS_SOURCES
    HatchetSampleProfileController.cpp)
endif()

add_library(caliper-controllers OBJECT ${CALIPER_CONTROLLERS_SOURCES})

if (BUILD_SHARED_LIBS)
  set_property(TARGET caliper-controllers PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()
