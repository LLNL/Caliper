set(CALIPER_RUNTIME_SOURCES
  Annotation.cpp
  AnnotationBinding.cpp
  AsyncEvent.cpp
  Blackboard.cpp
  Caliper.cpp
  ChannelController.cpp
  ConfigManager.cpp
  CustomOutputController.cpp
  MemoryPool.cpp
  MetadataTree.cpp
  RegionFilter.cpp
  RegionProfile.cpp
  api.cpp
  builtin_configmanager.cpp
  cali.cpp
  cali_datatracker.cpp
  config_sanity_check.cpp)

if (CALIPER_HAVE_MPI)
  list(APPEND CALIPER_RUNTIME_SOURCES
    aggregate_over_mpi.cpp
    collective_flush.cpp
    CollectiveOutputChannel.cpp
    OutputCommMpi.cpp
    MpiChannelManager.cpp
    machine_mpi.cpp
    setup_mpi.cpp)
else()
  list(APPEND CALIPER_RUNTIME_SOURCES
    machine_serial.cpp
    setup_serial.cpp)
endif()

add_subdirectory(controllers)

add_library(caliper-runtime OBJECT ${CALIPER_RUNTIME_SOURCES})

target_compile_options(caliper-runtime PRIVATE ${Wall_flag})
target_compile_features(caliper-runtime PUBLIC cxx_std_11)

if (CALIPER_HAVE_MPI)
  target_include_directories(caliper-runtime PRIVATE ${MPI_CXX_INCLUDE_PATH})
endif()

if (BUILD_TESTING)
  add_subdirectory(test)
endif()
