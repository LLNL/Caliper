include_directories("../common")
include_directories ("../services")

set(CALIPER_HEADERS
    Annotation.h
    CaliFunctional.h
    Caliper.h
    EntryList.h
    cali.h
    cali_definitions.h)

set(CALIPER_SOURCES
    Annotation.cpp
    Caliper.cpp
    ContextBuffer.cpp
    EntryList.cpp
    MemoryPool.cpp
    MetadataTree.cpp
    cali.cpp)


add_library(caliper ${CALIPER_SOURCES} $<TARGET_OBJECTS:caliper-services> ${CALIPER_SERVICES_LIBS})
target_link_libraries(caliper caliper-common)
target_link_libraries(caliper ${CMAKE_THREAD_LIBS_INIT})

foreach (_extlib ${CALIPER_EXTERNAL_LIBS})
  target_link_libraries(caliper ${_extlib})
endforeach()

install(FILES ${CALIPER_HEADERS} DESTINATION include/caliper)

install(TARGETS caliper 
  EXPORT caliper 
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)
