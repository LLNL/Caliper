include_directories("../../common")
include_directories("../../caliper")

include_directories(${MITOS_INCLUDE_DIR})

set(CALIPER_MITOS_SOURCES
    Mitos.cpp)

add_library(caliper-mitos ${CALIPER_MITOS_SOURCES})

target_link_libraries(caliper-mitos caliper-common)
target_link_libraries(caliper-mitos caliper)
target_link_libraries(caliper-mitos ${MITOS_LIBRARY})

install(TARGETS caliper-mitos DESTINATION lib)
