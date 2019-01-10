set(CALIPER_AGGREGATE_SOURCES
  Aggregate.cpp
  AggregationDB.cpp)

add_service_sources(${CALIPER_AGGREGATE_SOURCES})
add_caliper_service(aggregate)
