set(CALIPER_SOURCES
  Annotation.cpp
  AnnotationBinding.cpp
  Blackboard.cpp
  Caliper.cpp
  ChannelController.cpp
  ConfigManager.cpp
  SnapshotRecord.cpp
  MemoryPool.cpp
  MetadataTree.cpp
  api.cpp
  cali.cpp
  cali_datatracker.cpp
  config_sanity_check.cpp)

add_subdirectory(controllers)

add_library(caliper 
  $<TARGET_OBJECTS:util>
  $<TARGET_OBJECTS:caliper-common>

  $<TARGET_OBJECTS:caliper-reader>

  $<TARGET_OBJECTS:caliper-services>
  $<TARGET_OBJECTS:caliper-controllers>
  ${CALIPER_SOURCES} 
  ${CALIPER_SERVICES_LIBS}
  ${CALIPER_EXTERNAL_SOURCES})

set_target_properties(caliper PROPERTIES SOVERSION ${CALIPER_MAJOR_VERSION})
set_target_properties(caliper PROPERTIES VERSION ${CALIPER_VERSION})

target_link_libraries(caliper PRIVATE Threads::Threads)

foreach (_extlib ${CALIPER_EXTERNAL_LIBS})
  target_link_libraries(caliper PRIVATE ${_extlib})
endforeach()

if (BUILD_TESTING)
  add_subdirectory(test)
endif()

install(TARGETS caliper 
  EXPORT caliper 
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
