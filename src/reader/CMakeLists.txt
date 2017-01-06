include_directories ("../common")

set(CALIPER_READER_HEADERS
    Aggregator.h
    CaliperMetadataDB.h
    Expand.h
    Format.h
    RecordProcessor.h
    RecordSelector.h
    SimpleReader.h
    Table.h
    Json.h)

set(CALIPER_READER_SOURCES
    Aggregator.cpp
    Expand.cpp
    Format.cpp
    CaliperMetadataDB.cpp
    RecordSelector.cpp
    SimpleReader.cpp
    Table.cpp
    Json.cpp)

add_library(caliper-reader ${CALIPER_READER_SOURCES})

target_link_libraries(caliper-reader caliper-common)

install(FILES ${CALIPER_READER_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/caliper)

install(TARGETS caliper-reader 
  EXPORT caliper
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
