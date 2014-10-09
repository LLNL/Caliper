include_directories("../common")

set(CALIPER_SOURCES
    Annotation.cpp
    Caliper.cpp
    Context.cpp
    MemoryPool.cpp
    SigsafeRWLock.cpp)

add_library(caliper ${CALIPER_SOURCES})

target_link_libraries(caliper caliper-common)
target_link_libraries(caliper ${CMAKE_THREAD_LIBS_INIT})
