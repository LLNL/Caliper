include_directories(${OMPT_INCLUDE_DIR})

set(CALIPER_OMPT_SOURCES
    OmptService.cpp)

# add_library(caliper-ompt ${CALIPER_OMPT_SOURCES})

# target_link_libraries(caliper-ompt caliper-common)
# target_link_libraries(caliper-ompt caliper)

# install(TARGETS caliper-ompt DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(caliper-ompt OBJECT ${CALIPER_OMPT_SOURCES})

add_service_objlib("caliper-ompt")
add_caliper_service("ompt CALIPER_HAVE_OMPT")
