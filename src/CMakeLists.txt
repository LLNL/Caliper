
#
# Headers -- install them in $prefix/include
#

set(CALIPER_HEADERS
    Annotation.h
    Attribute.h
    Caliper.h
    IdType.h
    cali.h
    cali_types.h)

set(CALIPER_SOURCES
    Annotation.cpp
    Attribute.cpp
    AttributeStore.cpp
    Caliper.cpp
    Context.cpp
    ContextRecord.cpp
    MemoryPool.cpp
    Node.cpp
    Query.cpp
    SigsafeRWLock.cpp
    TextAttributeWriter.cpp
    TextNodeWriter.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")

add_library(caliper ${CALIPER_SOURCES})

target_link_libraries(caliper ${CMAKE_THREAD_LIBS_INIT})
