include_directories("../../common")

set(CALIPER_CSV_SOURCES
    Csv.cpp)

add_library(caliper-csv ${CALIPER_CSV_SOURCES})

target_link_libraries(caliper-csv caliper-common)

install(TARGETS caliper-csv DESTINATION lib)
