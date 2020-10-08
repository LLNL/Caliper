set(CALIPER_SERVICE_TEMPLATE_SOURCES
  MeasurementService.cpp)

# Use the add_service_sources() macro to add sources without external dependencies.
# For services that do have external dependencies (a third-party library), create
# an object library instead and use the add_service_objectlib macro instead:
#
#   include_directories(${<dependency>_INCLUDE_DIRS})
#   add_library(caliper-templateservices OBJECT ${CALIPER_SERVICE_TEMPLATE_SOURCES})
#
#   add_service_objlib("caliper-templateservices")
add_service_sources(${CALIPER_SERVICE_TEMPLATE_SOURCES})

# Use the add_caliper_service() macro to add the service(s).
# The first word in the string must be the name used in the service's 
# CaliperService variable name and name element. The second (optional) word
# is a #ifdef condition in caliper-config.h that must be met for the 
# service.
add_caliper_service("measurement_template CALIPER_BUILD_TESTING")
