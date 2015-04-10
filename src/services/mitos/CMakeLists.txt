include_directories("../../common")
include_directories("../../caliper")

include_directories($ENV{HOME}/testprefix/include)

set(CALIPER_MITOS_SOURCES
    Mitos.cpp)

add_library(caliper-mitos ${CALIPER_MITOS_SOURCES})

target_link_libraries(caliper-mitos caliper-common)
target_link_libraries(caliper-mitos caliper)
target_link_libraries(caliper-mitos mitos)

install(TARGETS caliper-mitos DESTINATION lib)
