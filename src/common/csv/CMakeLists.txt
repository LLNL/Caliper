include_directories("..")

set(CALIPER_CSV_SOURCES
    CsvReader.cpp
    CsvSpec.cpp)
set(CALIPER_CSV_HEADERS
    CsvReader.h
    CsvSpec.h)

add_library(caliper-csv OBJECT ${CALIPER_CSV_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-csv PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()

install(FILES ${CALIPER_CSV_HEADERS} DESTINATION include/caliper/csv)

