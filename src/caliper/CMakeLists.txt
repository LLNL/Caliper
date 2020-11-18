set(CALIPER_RUNTIME_SOURCES
  Annotation.cpp
  AnnotationBinding.cpp
  Blackboard.cpp
  Caliper.cpp
  ChannelController.cpp
  ConfigManager.cpp
  MemoryPool.cpp
  MetadataTree.cpp
  RegionProfile.cpp
  SnapshotRecord.cpp
  api.cpp
  builtin_configmanager.cpp
  cali.cpp
  cali_datatracker.cpp
  config_sanity_check.cpp)

add_subdirectory(controllers)

add_library(caliper-runtime OBJECT ${CALIPER_RUNTIME_SOURCES})

target_compile_options(caliper-runtime PRIVATE ${Wall_flag})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-runtime PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()

list(APPEND CALIPER_SERIAL_OBJS
  caliper/machine_serial.cpp
  caliper/setup_serial.cpp)

if (CALIPER_HAVE_MPI)
  list(APPEND CALIPER_ALL_OBJS
    caliper/machine_mpi.cpp
    caliper/setup_mpi.cpp)
else()
  list(APPEND CALIPER_ALL_OBJS
    caliper/machine_serial.cpp
    caliper/setup_serial.cpp)
endif()

set(CALIPER_SERIAL_OBJS ${CALIPER_SERIAL_OBJS} PARENT_SCOPE)
set(CALIPER_ALL_OBJS    ${CALIPER_ALL_OBJS}    PARENT_SCOPE)

if (BUILD_TESTING)
  add_subdirectory(test)
endif()
