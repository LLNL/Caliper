set(CALIPER_COMMON_HEADERS
    Attribute.h
    AttributeStore.h
    ContextRecord.h
    IdType.h
    Log.h
    Node.h
    RecordMap.h
    RuntimeConfig.h
    Variant.h
    cali_types.h)

set(CALIPER_COMMON_SOURCES
    Attribute.cpp
    AttributeStore.cpp
    ContextRecord.cpp
    Log.cpp
    Node.cpp
    RecordMap.cpp
    RuntimeConfig.cpp
    Variant.cpp)

add_library(caliper-common ${CALIPER_COMMON_SOURCES})

install(FILES ${CALIPER_COMMON_HEADERS} DESTINATION include)
install(TARGETS caliper-common DESTINATION lib)
