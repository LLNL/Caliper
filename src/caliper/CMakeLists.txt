set(CALIPER_SOURCES
  Annotation.cpp
  AnnotationBinding.cpp
  Blackboard.cpp
  Caliper.cpp
  ChannelController.cpp
  ConfigManager.cpp
  MemoryPool.cpp
  MetadataTree.cpp
  RegionProfile.cpp
  SnapshotRecord.cpp
  api.cpp
  cali.cpp
  cali_datatracker.cpp
  config_sanity_check.cpp)

add_subdirectory(controllers)

add_library(caliper-serial-obj OBJECT ${CALIPER_SOURCES})

target_compile_options(caliper-serial-obj PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-Wall>)

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-serial-obj PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()

set(CALIPER_SERIAL_OBJS
  $<TARGET_OBJECTS:caliper-util>
  $<TARGET_OBJECTS:caliper-common>

  $<TARGET_OBJECTS:caliper-reader>

  $<TARGET_OBJECTS:caliper-services>
  $<TARGET_OBJECTS:caliper-controllers>

  $<TARGET_OBJECTS:caliper-serial-obj>
  ${CALIPER_SERVICES_LIBS}
  ${CALIPER_EXTERNAL_SOURCES})

add_library(caliper-serial ${CALIPER_SERIAL_OBJS})

set_target_properties(caliper-serial PROPERTIES SOVERSION ${CALIPER_MAJOR_VERSION})
set_target_properties(caliper-serial PROPERTIES VERSION ${CALIPER_VERSION})

target_link_libraries(caliper-serial PRIVATE Threads::Threads)

foreach (_extlib ${CALIPER_EXTERNAL_LIBS})
  target_link_libraries(caliper-serial PRIVATE ${_extlib})
endforeach()

# Add serial objects and dependencies to global list for combined caliper lib
list(APPEND CALIPER_ALL_OBJS
  ${CALIPER_SERIAL_OBJS})
list(APPEND CALIPER_ALL_EXTERNAL_LIBS
  Threads::Threads
  ${CALIPER_EXTERNAL_LIBS})

set(CALIPER_ALL_OBJS ${CALIPER_ALL_OBJS} PARENT_SCOPE)
set(CALIPER_ALL_EXTERNAL_LIBS ${CALIPER_ALL_EXTERNAL_LIBS} PARENT_SCOPE)

if (BUILD_TESTING)
  add_subdirectory(test)
endif()

install(TARGETS caliper-serial
  EXPORT caliper 
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
