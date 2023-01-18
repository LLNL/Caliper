set(CALIPER_READER_SOURCES
  Aggregator.cpp
  CaliReader.cpp
  CaliWriter.cpp
  CaliperMetadataDB.cpp
  CalQLParser.cpp
  Expand.cpp
  FlatExclusiveRegionProfile.cpp
  FlatInclusiveRegionProfile.cpp
  FormatProcessor.cpp
  NestedExclusiveRegionProfile.cpp
  NestedInclusiveRegionProfile.cpp
  Preprocessor.cpp
  QueryProcessor.cpp
  QuerySpec.cpp
  RecordSelector.cpp
  SnapshotTableFormatter.cpp
  SnapshotTree.cpp
  TableFormatter.cpp
  TreeFormatter.cpp
  JsonFormatter.cpp
  JsonSplitFormatter.cpp
  UserFormatter.cpp)

add_library(caliper-reader OBJECT
  ${CALIPER_READER_SOURCES})

target_compile_options(caliper-reader PRIVATE ${Wall_flag})
target_compile_features(caliper-reader PUBLIC cxx_std_11)

if (BUILD_TESTING)
  add_subdirectory(test)
endif()
