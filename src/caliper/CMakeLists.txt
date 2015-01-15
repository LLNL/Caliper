include_directories("../common")
include_directories ("../services")

set(CALIPER_HEADERS
    Annotation.h
    Caliper.h
    cali.h
    cali_definitions.h)

set(CALIPER_SOURCES
    Annotation.cpp
    Caliper.cpp
    Context.cpp
    MemoryPool.cpp
    SigsafeRWLock.cpp
    cali.cpp)

add_library(caliper ${CALIPER_SOURCES})

target_link_libraries(caliper caliper-services)
target_link_libraries(caliper caliper-common)

target_link_libraries(caliper ${CMAKE_THREAD_LIBS_INIT})

install(FILES ${CALIPER_HEADERS} DESTINATION include)
install(TARGETS caliper DESTINATION lib)
