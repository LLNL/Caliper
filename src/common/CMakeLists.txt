set(CALIPER_COMMON_SOURCES
    Attribute.cpp
    AttributeStore.cpp
    ContextRecord.cpp
    Node.cpp
    Query.cpp)

add_library(caliper-common ${CALIPER_COMMON_SOURCES})
