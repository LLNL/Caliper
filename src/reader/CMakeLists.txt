include_directories ("../common")

set(CALIPER_READER_HEADERS
    Aggregator.h
    CaliperMetadataDB.h
    Expand.h
    RecordProcessor.h
    RecordSelector.h)

set(CALIPER_READER_SOURCES
    Aggregator.cpp
    Expand.cpp
    CaliperMetadataDB.cpp
    RecordSelector.cpp)

add_library(caliper-reader ${CALIPER_READER_SOURCES})

target_link_libraries(caliper-reader caliper-common)

install(FILES ${CALIPER_READER_HEADERS} DESTINATION include)

install(TARGETS caliper-reader 
  EXPORT caliper
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)
