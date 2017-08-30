set(CALIPER_READER_SOURCES
    Aggregator.cpp
    CalQLParser.cpp
    Expand.cpp
    FormatProcessor.cpp
    CaliperMetadataDB.cpp
    RecordSelector.cpp
    SimpleReader.cpp
    SnapshotTree.cpp
    TableFormatter.cpp
    TreeFormatter.cpp
    JsonFormatter.cpp
    UserFormatter.cpp)

add_library(caliper-reader ${CALIPER_READER_SOURCES})

target_link_libraries(caliper-reader caliper-common)

if (BUILD_TESTING)
  add_subdirectory(test)
endif()

install(TARGETS caliper-reader 
  EXPORT caliper
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
