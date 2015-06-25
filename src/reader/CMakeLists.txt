include_directories ("../common")

set(CALIPER_READER_HEADERS
    CaliperMetadataDB.h)

set(CALIPER_READER_SOURCES
    CaliperMetadataDB.cpp)

add_library(caliper-reader ${CALIPER_READER_SOURCES})

target_link_libraries(caliper-reader caliper-common)

install(FILES ${CALIPER_READER_HEADERS} DESTINATION include)
install(TARGETS caliper-reader DESTINATION lib)
