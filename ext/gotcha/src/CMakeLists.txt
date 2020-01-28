set(GOTCHA_SOURCES
  gotcha_utils.c 
  gotcha.c 
  gotcha_auxv.c 
  libc_wrappers.c 
  elf_ops.c 
  hash.c 
  tool.c 
  library_filters.c
  gotcha_dl.c
  translations.c
)

add_library(caliper-gotcha OBJECT ${GOTCHA_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-gotcha PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()

target_include_directories(caliper-gotcha PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
