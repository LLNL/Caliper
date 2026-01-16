set(CALIPER_LDMS_SOURCES
  LdmsForwarder.cpp)

add_library(caliper-ldms OBJECT ${CALIPER_LDMS_SOURCES})
target_include_directories(caliper-ldms PRIVATE ${LDMS_INCLUDE_DIRS})

# Add compiler flags to handle LDMS header compatibility issues
target_compile_options(caliper-ldms PRIVATE -fpermissive)

add_service_objlib("caliper-ldms")
add_caliper_service("ldms CALIPER_HAVE_LDMS")
