set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

include(CheckCXXCompilerFlag)
set(Wall_flag)
check_cxx_compiler_flag("-Wall" Supports_Wall_Flag)
if(Supports_Wall_Flag)
  set(Wall_flag "-Wall")
endif()

include_directories(${PROJECT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/interface/c_fortran)

add_subdirectory(common)
add_subdirectory(interface/c_fortran)
add_subdirectory(reader)
add_subdirectory(services)
add_subdirectory(caliper)

set(CALIPER_COMMON_OBJS
  $<TARGET_OBJECTS:caliper-common>
  $<TARGET_OBJECTS:caliper-interface>
  $<TARGET_OBJECTS:caliper-reader>
  $<TARGET_OBJECTS:caliper-services>
  $<TARGET_OBJECTS:caliper-runtime>
  ${CALIPER_SERVICES_LIBS}
  ${CALIPER_EXTERNAL_SOURCES})

list(APPEND CALIPER_ALL_OBJS
  ${CALIPER_COMMON_OBJS})

add_library(caliper ${CALIPER_ALL_OBJS})

set_target_properties(caliper PROPERTIES SOVERSION ${CALIPER_MAJOR_VERSION})
set_target_properties(caliper PROPERTIES VERSION ${CALIPER_VERSION})

target_compile_features(caliper PUBLIC cxx_std_11)

target_include_directories(
  caliper
  INTERFACE
  "$<INSTALL_INTERFACE:include>")

if (CALIPER_HAVE_MPI)
  target_link_libraries(caliper PUBLIC ${MPI_CXX_LIBRARIES})
endif()

foreach (_extlib ${CALIPER_EXTERNAL_LIBS})
  target_link_libraries(caliper PRIVATE ${_extlib})
endforeach()

if (WIN32)
  target_link_libraries(caliper PUBLIC ws2_32)
endif()

if (WITH_TOOLS)
  add_subdirectory(tools)
endif()

install(TARGETS caliper
  EXPORT caliper-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
