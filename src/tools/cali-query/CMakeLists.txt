include_directories ("../../common")
include_directories ("../../services/csv")

set(CALIPER_QUERY_SOURCES
    CaliperMetadataDB.cpp
    cali-query.cpp)

add_executable(cali-query ${CALIPER_QUERY_SOURCES})

target_link_libraries(cali-query caliper-common)
target_link_libraries(cali-query caliper-csv)

install(TARGETS cali-query DESTINATION bin)
