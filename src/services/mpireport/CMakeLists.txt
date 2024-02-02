include_directories(${MPI_CXX_INCLUDE_PATH})
add_service_sources(MpiReport.cpp)
add_caliper_service("mpireport CALIPER_HAVE_MPI")