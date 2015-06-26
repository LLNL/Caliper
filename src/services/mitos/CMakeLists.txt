
message(STATUS "Mitos include directory: ${Mitos_INCLUDE_DIRS}")
include_directories(${Mitos_INCLUDE_DIRS})

set(CALIPER_MITOS_SOURCES
    Mitos.cpp)

# add_library(caliper-mitos ${CALIPER_MITOS_SOURCES})

# target_link_libraries(caliper-mitos caliper-common)
# target_link_libraries(caliper-mitos caliper)

# install(TARGETS caliper-mitos DESTINATION lib)

add_service_sources(${CALIPER_MITOS_SOURCES})
