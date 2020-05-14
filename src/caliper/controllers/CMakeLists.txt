set(CALIPER_CONTROLLERS_SOURCES
  controllers/CudaActivityController.cpp
  controllers/HatchetRegionProfileController.cpp
  controllers/HatchetSampleProfileController.cpp
  controllers/RuntimeReportController.cpp
  controllers/controllers.cpp)

list(APPEND CALIPER_RUNTIME_SOURCES ${CALIPER_CONTROLLERS_SOURCES})
set(CALIPER_RUNTIME_SOURCES ${CALIPER_RUNTIME_SOURCES} PARENT_SCOPE)
