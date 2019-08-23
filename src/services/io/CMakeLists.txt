add_subdirectory(CurIOus/src)

set(CALIPER_IO_SOURCES
  IOService.cpp)

add_library(caliper-ioservice OBJECT ${CALIPER_IO_SOURCES})

target_include_directories(caliper-ioservice PRIVATE "CurIOus/include")

add_service_objlib("caliper-ioservice")
add_service_objlib("caliper-CurIOus")

add_caliper_service("io CALIPER_HAVE_GOTCHA")
