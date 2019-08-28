set(CALIPER_CurIOus_SOURCES
  callbacks.c
  curious.c
  dynamic_array.c
  file_registry.c
  mount_tree.c 
  wrappers.c)

add_library(caliper-CurIOus OBJECT ${CALIPER_CurIOus_SOURCES})

target_include_directories(caliper-CurIOus PRIVATE ${gotcha_INCLUDE_DIRS})
target_include_directories(caliper-CurIOus PRIVATE "../include")

set_target_properties(caliper-CurIOus PROPERTIES C_STANDARD 11)
set_target_properties(caliper-CurIOus PROPERTIES C_STANDARD_REQUIRED ON)
