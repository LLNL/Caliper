include_directories("../common")

set(CALIPER_SERVICES_SOURCES
    Services.cpp)

add_library(caliper-services ${CALIPER_SERVICES_SOURCES})

target_link_libraries(caliper-services caliper-debug)
target_link_libraries(caliper-services caliper-pthread)
target_link_libraries(caliper-services caliper-recorder)
target_link_libraries(caliper-services caliper-timestamp)
if (OMPT_FOUND)
  target_link_libraries(caliper-services caliper-ompt)
endif()
target_link_libraries(caliper-services caliper-common)

install(TARGETS caliper-services DESTINATION lib)

add_subdirectory(csv)
add_subdirectory(debug)
add_subdirectory(pthread)
add_subdirectory(recorder)
add_subdirectory(timestamp)
if (OMPT_FOUND)
  add_subdirectory(ompt)
endif()
