set(CALIPER_CONTROLLERS_SOURCES
  CudaActivityController.cpp
  HatchetRegionProfileController.cpp
  HatchetSampleProfileController.cpp
  RuntimeReportController.cpp
  controllers.cpp)

add_library(caliper-controllers OBJECT ${CALIPER_CONTROLLERS_SOURCES})

if (BUILD_SHARED_LIBS)
  set_property(TARGET caliper-controllers PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()
