set(CALIPER_COMMON_HEADERS
    Attribute.h
    CaliperMetadataAccessInterface.h
    ContextRecord.h
    Entry.h
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
    util/lockfree-tree.hpp
    util/shared_obj.hpp)

set(CALIPER_COMMON_SOURCES
    Attribute.cpp
    ContextRecord.cpp
    Entry.cpp
    Log.cpp
    Node.cpp
    RecordMap.cpp
    RuntimeConfig.cpp
    Variant.cpp
    cali_types.c)

add_subdirectory(csv)
add_subdirectory(c-util)

add_library(caliper-common
  $<TARGET_OBJECTS:caliper-csv>
  $<TARGET_OBJECTS:c-util>
  ${CALIPER_COMMON_SOURCES})

install(FILES ${CALIPER_COMMON_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/caliper)
install(FILES ${CALIPER_UTIL_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/caliper/util)

install(TARGETS caliper-common 
  EXPORT caliper
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
