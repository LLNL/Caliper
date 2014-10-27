include_directories("../common")
include_directories ("../services/csv")

set(CALIPER_SOURCES
    Annotation.cpp
    Caliper.cpp
    Context.cpp
    MemoryPool.cpp
    SigsafeRWLock.cpp)

add_library(caliper ${CALIPER_SOURCES})

target_link_libraries(caliper caliper-csv)
target_link_libraries(caliper caliper-common)

target_link_libraries(caliper ${CMAKE_THREAD_LIBS_INIT})
