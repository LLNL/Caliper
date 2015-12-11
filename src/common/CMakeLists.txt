set(CALIPER_COMMON_HEADERS
    Attribute.h
    ContextRecord.h
    IdType.h
    Log.h
    Node.h
    Record.h
    RecordMap.h
    RuntimeConfig.h
    Variant.h
    cali_types.h)
set(CALIPER_UTIL_HEADERS
    util/callback.hpp
    util/list.hpp
    util/split.hpp
    util/tree.hpp)

set(CALIPER_COMMON_SOURCES
    Attribute.cpp
    ContextRecord.cpp
    Log.cpp
    Node.cpp
    RecordMap.cpp
    RuntimeConfig.cpp
    Variant.cpp
    cali_types.c)

add_subdirectory(csv)

add_library(caliper-common $<TARGET_OBJECTS:caliper-csv> ${CALIPER_COMMON_SOURCES})

install(FILES ${CALIPER_COMMON_HEADERS} DESTINATION include)
install(FILES ${CALIPER_UTIL_HEADERS} DESTINATION include/util)

install(TARGETS caliper-common 
  EXPORT caliper
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)
