include_directories(${Mitos_INCLUDE_DIRS})

set(CALIPER_MITOS_SOURCES
    Mitos.cpp)

add_library(caliper-mitos OBJECT ${CALIPER_MITOS_SOURCES})

add_service_objlib("caliper-mitos")
