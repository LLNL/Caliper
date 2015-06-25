include_directories("..")

set(CALIPER_CSV_SOURCES
    CsvReader.cpp
    CsvSpec.cpp)
set(CALIPER_CSV_HEADERS
    CsvReader.h
    CsvSpec.h)

add_library(caliper-csv OBJECT ${CALIPER_CSV_SOURCES})

install(FILES ${CALIPER_CSV_HEADERS} DESTINATION include/csv)

