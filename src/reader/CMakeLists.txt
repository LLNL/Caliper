set(CALIPER_READER_SOURCES
    Aggregator.cpp
    CaliReader.cpp
    CaliWriter.cpp
    CalQLParser.cpp
    Expand.cpp
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

add_library(caliper-reader ${CALIPER_READER_SOURCES})

set_target_properties(caliper-reader PROPERTIES SOVERSION ${CALIPER_MAJOR_VERSION})
set_target_properties(caliper-reader PROPERTIES VERSION ${CALIPER_VERSION})

target_link_libraries(caliper-reader caliper-common)

if (BUILD_TESTING)
  add_subdirectory(test)
endif()

install(TARGETS caliper-reader 
  EXPORT caliper
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
