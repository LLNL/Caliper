include_directories(${LIBUNWIND_INCLUDE_DIR})

set(CALIPER_CALLPATH_SOURCES
    Callpath.cpp)

# install(TARGETS caliper-callpath DESTINATION lib)

add_library(caliper-callpath OBJECT ${CALIPER_CALLPATH_SOURCES})

add_service_objlib("$<TARGET_OBJECTS:caliper-callpath>")

# target_link_libraries(caliper ${LIBUNWIND_LIBRARY})