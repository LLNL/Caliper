set(CALIPER_READER_SOURCES
    Aggregator.cpp
    Expand.cpp
    Format.cpp
    CaliperMetadataDB.cpp
    RecordSelector.cpp
    SimpleReader.cpp
    Table.cpp
    TreeFormatter.cpp
    Json.cpp)

add_library(caliper-reader ${CALIPER_READER_SOURCES})

target_link_libraries(caliper-reader caliper-common)

install(TARGETS caliper-reader 
  EXPORT caliper
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
