set(CALIPER_READER_SOURCES
  Aggregator.cpp
  CaliReader.cpp
  CaliWriter.cpp
  CalQLParser.cpp
  Expand.cpp
  FlatExclusiveRegionProfile.cpp
  FormatProcessor.cpp
  CaliperMetadataDB.cpp
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

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-reader PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()

if (BUILD_TESTING)
  add_subdirectory(test)
endif()
