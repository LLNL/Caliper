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
  SnapshotTree.cpp
  TableFormatter.cpp
  TreeFormatter.cpp
  JsonFormatter.cpp
  JsonSplitFormatter.cpp
  UserFormatter.cpp)

add_library(caliper-reader OBJECT
  ${CALIPER_READER_SOURCES})

target_compile_options(caliper-reader PRIVATE ${Wall_flag})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-reader PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()

if (BUILD_TESTING)
  add_subdirectory(test)
endif()
