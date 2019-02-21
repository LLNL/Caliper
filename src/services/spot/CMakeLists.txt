set(CALIPER_SPOT_SOURCES
  Spot.cpp)

add_library(caliper-spot OBJECT ${CALIPER_SPOT_SOURCES})

add_service_objlib("caliper-spot")
add_caliper_service("spot")
