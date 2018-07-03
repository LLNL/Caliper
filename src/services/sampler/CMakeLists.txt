set(CALIPER_SAMPLER_SOURCES
    Sampler.cpp)

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES x86)
  set(CALI_HAVE_CONTEXT_H true)
  set(CALI_CONTEXT_H "context_x86.h")
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES ppc)
  set(CALI_HAVE_CONTEXT_H true)
  set(CALI_CONTEXT_H "context_ppc.h")
else()
  message(WARNING "Unknown architecture ${CMAKE_SYSTEM_PROCESSOR}")
endif()
  
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/context.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/context.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

add_library(caliper-sampler OBJECT ${CALIPER_SAMPLER_SOURCES})

add_service_objlib("caliper-sampler")
add_caliper_service("sampler CALIPER_HAVE_SAMPLER")
