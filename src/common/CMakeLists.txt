set(CALIPER_COMMON_SOURCES
    Attribute.cpp
    AttributeStore.cpp
    ContextRecord.cpp
    Node.cpp
    RecordMap.cpp
    Variant.cpp)

add_library(caliper-common ${CALIPER_COMMON_SOURCES})
