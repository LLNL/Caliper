set(CALIPER_COMMON_SOURCES
    Attribute.cpp
    CaliperMetadataAccessInterface.cpp
    CompressedSnapshotRecord.cpp
    ContextRecord.cpp
    Entry.cpp
    Log.cpp
    Node.cpp
    NodeBuffer.cpp
    OutputStream.cpp
    RecordMap.cpp
    RuntimeConfig.cpp
    SnapshotBuffer.cpp
    SnapshotTextFormatter.cpp
    StringConverter.cpp
    Variant.cpp
    cali_types.c
    cali_variant.c)

add_subdirectory(csv)
add_subdirectory(c-util)

add_library(caliper-common
  $<TARGET_OBJECTS:caliper-csv>
  $<TARGET_OBJECTS:c-util>
  ${CALIPER_COMMON_SOURCES})

set_target_properties(caliper-common PROPERTIES SOVERSION ${CALIPER_MAJOR_VERSION})
set_target_properties(caliper-common PROPERTIES VERSION ${CALIPER_VERSION})

if (BUILD_TESTING)
  add_subdirectory(test)
endif()

install(TARGETS caliper-common 
  EXPORT caliper
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
