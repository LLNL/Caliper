include_directories(${LIBUNWIND_INCLUDE_DIR})

set(CALIPER_CALLPATH_SOURCES
    Callpath.cpp)

# add_library(caliper-callpath ${CALIPER_CALLPATH_SOURCES})
# install(TARGETS caliper-callpath DESTINATION lib)

add_service_sources(${CALIPER_CALLPATH_SOURCES})
# target_link_libraries(caliper ${LIBUNWIND_LIBRARY})
