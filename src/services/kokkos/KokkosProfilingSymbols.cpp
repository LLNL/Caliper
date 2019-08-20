#include <cstdint>
#include "types.hpp"
#include "caliper/Annotation.h"
#include "caliper/cali.h"

cali::kokkos::callbacks kokkosp_callbacks;

using cali::kokkos::SpaceHandle;

extern "C" void kokkosp_init_library(const int loadSeq,
  const uint64_t interfaceVer,
  const uint32_t devInfoCount,
  void* deviceInfo) {
    cali::Annotation("caliper_initialization_trigger",CALI_ATTR_HIDDEN).begin();
    cali::Annotation("callback_initialization_trigger",CALI_ATTR_HIDDEN).end();
    kokkosp_callbacks.kokkosp_init_callback(loadSeq,interfaceVer,devInfoCount, deviceInfo);
}

extern "C" void kokkosp_finalize_library() {
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

extern "C" void kokkosp_end_parallel_for(const uint64_t kID) {
    kokkosp_callbacks.kokkosp_end_parallel_for_callback(kID);
}
extern "C" void kokkosp_end_parallel_reduce(const uint64_t kID) {
    kokkosp_callbacks.kokkosp_end_parallel_reduce_callback(kID);
}
extern "C" void kokkosp_end_parallel_scan(const uint64_t kID) {
    kokkosp_callbacks.kokkosp_end_parallel_scan_callback(kID);
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
