#include <cstdint>
#include "types.hpp"
#include "caliper/Annotation.h"
#include "caliper/cali.h"
#include "caliper/ConfigManager.h"
cali::kokkos::callbacks kokkosp_callbacks;

using cali::kokkos::SpaceHandle;
cali::ConfigManager mgr;
extern "C" void kokkosp_parse_args(int argc, char* argv_raw[]){
  std::string joined_argv;
  for(int x = 1 ; x< argc; ++x){
    joined_argv += (std::string(argv_raw[x])) + (( x == (argc-1) ) ? std::string("") : std::string(","));
  }
  if(argc > 1) {
  mgr.add(joined_argv.c_str());
  if(mgr.error()){
    std::cerr << "Kokkos Caliper connector error: "<< mgr.error_msg() << std::endl;
  }
  mgr.start();
  }
  
}

extern "C" void kokkosp_init_library(const int loadSeq,
  const uint64_t interfaceVer,
  const uint32_t devInfoCount,
  void* deviceInfo) {
    cali_init();
    kokkosp_callbacks.kokkosp_init_callback(loadSeq,interfaceVer,devInfoCount, deviceInfo);
}

extern "C" void kokkosp_finalize_library() {
    mgr.flush();
    kokkosp_callbacks.kokkosp_finalize_callback();
}

extern "C" void kokkosp_begin_parallel_for(const char* name, const uint32_t devID, uint64_t* kID) {
    kokkosp_callbacks.kokkosp_begin_parallel_for_callback(name, devID, kID);
}
extern "C" void kokkosp_begin_parallel_reduce(const char* name, const uint32_t devID, uint64_t* kID) {
    kokkosp_callbacks.kokkosp_begin_parallel_reduce_callback(name, devID, kID);
}
extern "C" void kokkosp_begin_parallel_scan(const char* name, const uint32_t devID, uint64_t* kID) {
    kokkosp_callbacks.kokkosp_begin_parallel_scan_callback(name, devID, kID);
}
extern "C" void kokkosp_begin_fence(const char* name, const uint32_t devID, uint64_t* kID) {
    kokkosp_callbacks.kokkosp_begin_fence_callback(name, devID, kID);
}

extern "C" void kokkosp_end_parallel_for(const uint64_t kID) {
    kokkosp_callbacks.kokkosp_end_parallel_for_callback(kID);
}
extern "C" void kokkosp_end_parallel_reduce(const uint64_t kID) {
    kokkosp_callbacks.kokkosp_end_parallel_reduce_callback(kID);
}
extern "C" void kokkosp_end_parallel_scan(const uint64_t kID) {
    kokkosp_callbacks.kokkosp_end_parallel_scan_callback(kID);
}
extern "C" void kokkosp_end_fence(const uint64_t kID) {
    kokkosp_callbacks.kokkosp_end_fence_callback(kID);
}

extern "C" void kokkosp_push_profile_region(char* regionName) {
    kokkosp_callbacks.kokkosp_push_region_callback(regionName);
}
extern "C" void kokkosp_pop_profile_region() {
    kokkosp_callbacks.kokkosp_pop_region_callback();
}
extern "C" void kokkosp_allocate_data(const SpaceHandle space, const char* label, const void* const ptr, const uint64_t size) {
    kokkosp_callbacks.kokkosp_allocate_callback(space,label,ptr,size);
}
extern "C" void kokkosp_deallocate_data(const SpaceHandle space, const char* label, const void* const ptr, const uint64_t size) {
    kokkosp_callbacks.kokkosp_deallocate_callback(space,label,ptr,size);
}

extern "C" void kokkosp_begin_deep_copy(const SpaceHandle dst_handle, const char* dst_name, const void* dst_ptr,
    const SpaceHandle src_space, const char* src_name, const void* src_ptr,
    const uint64_t size) {
    kokkosp_callbacks.kokkosp_begin_deep_copy_callback(dst_handle, dst_name,dst_ptr,src_space,src_name,src_ptr,size);
}
extern "C" void kokkosp_end_deep_copy() {
    kokkosp_callbacks.kokkosp_end_deep_copy_callback();
}
