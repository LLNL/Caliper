set(CALIPER_CSV_SOURCES
    CsvReader.cpp
    CsvWriter.cpp)

add_library(caliper-csv OBJECT ${CALIPER_CSV_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-csv PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()


