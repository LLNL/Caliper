set(CALIPER_COMMON_SOURCES
  Attribute.cpp
  CaliperMetadataAccessInterface.cpp
  CompressedSnapshotRecord.cpp
  Entry.cpp
  Log.cpp
  NodeBuffer.cpp
  OutputStream.cpp
  RuntimeConfig.cpp
  SnapshotBuffer.cpp
  SnapshotTextFormatter.cpp
  StringConverter.cpp
  Variant.cpp
  cali_types.c
  cali_variant.c)

add_subdirectory(util)

add_library(caliper-common OBJECT
  ${CALIPER_COMMON_SOURCES})

target_compile_options(caliper-common PRIVATE ${Wall_flag})
target_compile_features(caliper-common PUBLIC cxx_std_11 c_std_99)

if (BUILD_TESTING)
  add_subdirectory(test)
endif()
