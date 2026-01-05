set(CALIPER_CONTROLLERS_SOURCES
  controllers/CudaActivityProfileController.cpp
  controllers/CudaActivityReportController.cpp
  controllers/LoopReportController.cpp
  controllers/OpenMPReportController.cpp
  controllers/ROCmActivityProfileController.cpp
  controllers/ROCmActivityReportController.cpp
  controllers/RuntimeProfileController.cpp
  controllers/RuntimeReportController.cpp
  controllers/SampleProfileController.cpp
  controllers/SampleReportController.cpp
  controllers/SpotController.cpp
  controllers/controllers.cpp
  controllers/util.cpp)

list(APPEND CALIPER_RUNTIME_SOURCES ${CALIPER_CONTROLLERS_SOURCES})
set(CALIPER_RUNTIME_SOURCES ${CALIPER_RUNTIME_SOURCES} PARENT_SCOPE)
