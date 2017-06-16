set(CALIPER_SOURCES
    Annotation.cpp
    Caliper.cpp
    CompressedSnapshotRecord.cpp
    ContextBuffer.cpp
    SnapshotRecord.cpp
    MemoryPool.cpp
    MetadataTree.cpp
    api.cpp
    cali.cpp)

add_library(caliper ${CALIPER_SOURCES} $<TARGET_OBJECTS:caliper-services> ${CALIPER_SERVICES_LIBS})
target_link_libraries(caliper PUBLIC caliper-common)
target_link_libraries(caliper PRIVATE ${CMAKE_THREAD_LIBS_INIT})

foreach (_extlib ${CALIPER_EXTERNAL_LIBS})
  target_link_libraries(caliper PRIVATE ${_extlib})
endforeach()

if (BUILD_TESTING)
  add_subdirectory(test)
endif()

# Add reader lib to runtime (for report service)
target_link_libraries(caliper PUBLIC caliper-reader)

install(TARGETS caliper 
  EXPORT caliper 
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
