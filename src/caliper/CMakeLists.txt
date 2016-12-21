include_directories("../common")
include_directories ("../services")

set(CALIPER_HEADERS
    Annotation.h
    CaliFunctional.h
    Caliper.h
    SnapshotRecord.h
    cali.h
    cali_definitions.h)

set(CALIPER_SOURCES
    Annotation.cpp
    Caliper.cpp
    ContextBuffer.cpp
    SnapshotRecord.cpp
    MemoryPool.cpp
    MetadataTree.cpp
    cali.cpp)


add_library(caliper ${CALIPER_SOURCES} $<TARGET_OBJECTS:caliper-services> ${CALIPER_SERVICES_LIBS})
target_link_libraries(caliper caliper-common)
target_link_libraries(caliper ${CMAKE_THREAD_LIBS_INIT})

foreach (_extlib ${CALIPER_EXTERNAL_LIBS})
  target_link_libraries(caliper ${_extlib})
endforeach()

install(FILES ${CALIPER_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/caliper)

install(TARGETS caliper 
  EXPORT caliper 
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
