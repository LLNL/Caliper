#include "impl/Kokkos_Profiling_Interface.hpp"
#include "caliper/common/interfaces/kokkos/CaliperKokkos.h"
namespace cali {

namespace kokkos {
using Kokkos::Profiling::SpaceHandle;
using Kokkos::Profiling::Experimental::EventSet;
extern "C" void kokkosp_init_library();
extern "C" void kokkosp_finalize_library() ;
extern "C" void kokkosp_begin_parallel_for(const char* name, const uint32_t devID, uint64_t* kID) ;
extern "C" void kokkosp_begin_parallel_reduce(const char* name, const uint32_t devID, uint64_t* kID) ;
extern "C" void kokkosp_begin_parallel_scan(const char* name, const uint32_t devID, uint64_t* kID) ;
extern "C" void kokkosp_end_parallel_for(const uint64_t kID) ;
extern "C" void kokkosp_end_parallel_reduce(const uint64_t kID) ;
extern "C" void kokkosp_end_parallel_scan(const uint64_t kID) ;
extern "C" void kokkosp_push_profile_region(const char* regionName) ;
extern "C" void kokkosp_pop_profile_region() ;
extern "C" void kokkosp_allocate_data(const SpaceHandle space, const char* label, const void* const ptr, const uint64_t size) ;
extern "C" void kokkosp_deallocate_data(const SpaceHandle space, const char* label, const void* const ptr, const uint64_t size) ;
extern "C" void kokkosp_begin_deep_copy(const SpaceHandle dst_handle, const char* dst_name, const void* dst_ptr,
    const SpaceHandle src_space, const char* src_name, const void* src_ptr,
    const uint64_t size);
extern "C" void kokkosp_end_deep_copy();
EventSet get_event_set(size_t components){
  EventSet events;

  if(components & ProfilingComponents::fors){
    events.begin_parallel_for = kokkosp_begin_parallel_for;
    events.end_parallel_for = kokkosp_end_parallel_for;
  } 
  if(components & ProfilingComponents::reduces){
    events.begin_parallel_reduce = kokkosp_begin_parallel_reduce;
    events.end_parallel_reduce = kokkosp_end_parallel_reduce;
  } 
  if(components & ProfilingComponents::scans){
    events.begin_parallel_scan = kokkosp_begin_parallel_scan;
    events.end_parallel_scan = kokkosp_end_parallel_scan;
  } 
  if(components & ProfilingComponents::copies){
    events.begin_deep_copy = kokkosp_begin_deep_copy;
    events.end_deep_copy = kokkosp_end_deep_copy;
  } 
  if(components & ProfilingComponents::regions){
    events.push_region = kokkosp_push_profile_region;
    events.pop_region = kokkosp_pop_profile_region;
  } 
  return events;
}

}
}
