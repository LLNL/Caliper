#include "caliper/Annotation.h"
#include "caliper/ConfigManager.h"
#include "caliper/cali.h"
#include "types.hpp"
#include <impl/Kokkos_Profiling_Interface.hpp>
#include <cstdint>
cali::kokkos::callbacks kokkosp_callbacks;
using Kokkos::Tools::SpaceHandle;
namespace kokkos {
    cali::ConfigManager mgr;
}
namespace cali {
void kokkosp_print_help(char* progName){
  std::cerr << "Caliper: available configs: \n";
  for(auto conf: ::kokkos::mgr.available_config_specs() ) {
    std::cerr << ::kokkos::mgr.get_documentation_for_spec(conf.c_str()) << std::endl;
  }
}
void kokkosp_parse_args(int argc, char *argv_raw[]) {
  if (argc > 2) {
    std::cerr << "Error: the Kokkos Caliper connector takes only one argument"
              << std::endl;
  }
  if (argc == 2) {
    ::kokkos::mgr.add(argv_raw[1]);
    if (::kokkos::mgr.error()) {
      std::cerr << "Kokkos Caliper connector error: " << ::kokkos::mgr.error_msg()
                << std::endl;
    }
    ::kokkos::mgr.start();
  }
}

void kokkosp_init_library(const int loadSeq,
                                     const uint64_t interfaceVer,
                                     const uint32_t devInfoCount,
                                     Kokkos::Profiling::KokkosPDeviceInfo *deviceInfo) {
  cali_init();
  kokkosp_callbacks.kokkosp_init_callback(loadSeq, interfaceVer, devInfoCount,
                                          deviceInfo);
}

void kokkosp_finalize_library() {
  ::kokkos::mgr.flush();
  kokkosp_callbacks.kokkosp_finalize_callback();
}

void kokkosp_begin_parallel_for(const char *name,
                                           const uint32_t devID,
                                           uint64_t *kID) {
  kokkosp_callbacks.kokkosp_begin_parallel_for_callback(name, devID, kID);
}
void kokkosp_begin_parallel_reduce(const char *name,
                                              const uint32_t devID,
                                              uint64_t *kID) {
  kokkosp_callbacks.kokkosp_begin_parallel_reduce_callback(name, devID, kID);
}
void kokkosp_begin_parallel_scan(const char *name,
                                            const uint32_t devID,
                                            uint64_t *kID) {
  kokkosp_callbacks.kokkosp_begin_parallel_scan_callback(name, devID, kID);
}
void kokkosp_begin_fence(const char *name, const uint32_t devID,
                                    uint64_t *kID) {
  kokkosp_callbacks.kokkosp_begin_fence_callback(name, devID, kID);
}

void kokkosp_end_parallel_for(const uint64_t kID) {
  kokkosp_callbacks.kokkosp_end_parallel_for_callback(kID);
}
void kokkosp_end_parallel_reduce(const uint64_t kID) {
  kokkosp_callbacks.kokkosp_end_parallel_reduce_callback(kID);
}
void kokkosp_end_parallel_scan(const uint64_t kID) {
  kokkosp_callbacks.kokkosp_end_parallel_scan_callback(kID);
}
void kokkosp_end_fence(const uint64_t kID) {
  kokkosp_callbacks.kokkosp_end_fence_callback(kID);
}

void kokkosp_push_profile_region(const char *regionName) {
  kokkosp_callbacks.kokkosp_push_region_callback(regionName);
}
void kokkosp_pop_profile_region() {
  kokkosp_callbacks.kokkosp_pop_region_callback();
}
void kokkosp_allocate_data(const SpaceHandle space,
                                      const char *label, const void *const ptr,
                                      const uint64_t size) {
  kokkosp_callbacks.kokkosp_allocate_callback(space, label, ptr, size);
}
void kokkosp_deallocate_data(const SpaceHandle space,
                                        const char *label,
                                        const void *const ptr,
                                        const uint64_t size) {
  kokkosp_callbacks.kokkosp_deallocate_callback(space, label, ptr, size);
}

void
kokkosp_begin_deep_copy(const SpaceHandle dst_handle, const char *dst_name,
                        const void *dst_ptr, const SpaceHandle src_space,
                        const char *src_name, const void *src_ptr,
                        const uint64_t size) {
  kokkosp_callbacks.kokkosp_begin_deep_copy_callback(
      dst_handle, dst_name, dst_ptr, src_space, src_name, src_ptr, size);
}
void kokkosp_end_deep_copy() {
  kokkosp_callbacks.kokkosp_end_deep_copy_callback();
}
} // end namespace cali

extern "C" {

__attribute__((weak)) void kokkosp_print_help(char* progName){
  cali::kokkosp_print_help(progName);
}

__attribute__((weak)) void kokkosp_parse_args(int argc, char *argv_raw[]) {
  cali::kokkosp_parse_args(argc, argv_raw);
}

__attribute__((weak)) void kokkosp_init_library(const int loadSeq,
                                     const uint64_t interfaceVer,
                                     const uint32_t devInfoCount,
                                     Kokkos::Profiling::KokkosPDeviceInfo *deviceInfo) {
  cali::kokkosp_init_library(loadSeq, interfaceVer, devInfoCount, deviceInfo);
}

__attribute__((weak)) void kokkosp_finalize_library() {
  cali::kokkosp_finalize_library();
}

__attribute__((weak)) void kokkosp_begin_parallel_for(const char *name,
                                           const uint32_t devID,
                                           uint64_t *kID) {
  cali::kokkosp_begin_parallel_for(name, devID, kID);
}
__attribute__((weak)) void kokkosp_begin_parallel_reduce(const char *name,
                                              const uint32_t devID,
                                              uint64_t *kID) {
  cali::kokkosp_begin_parallel_reduce(name, devID, kID);
  
}
__attribute__((weak)) void kokkosp_begin_parallel_scan(const char *name,
                                            const uint32_t devID,
                                            uint64_t *kID) {
  cali::kokkosp_begin_parallel_scan(name, devID, kID);
}
__attribute__((weak)) void kokkosp_begin_fence(const char *name, const uint32_t devID,
                                    uint64_t *kID) {
  cali::kokkosp_begin_fence(name, devID, kID);
}

__attribute__((weak)) void kokkosp_end_parallel_for(const uint64_t kID) {
  cali::kokkosp_end_parallel_for(kID);
}
__attribute__((weak)) void kokkosp_end_parallel_reduce(const uint64_t kID) {
  cali::kokkosp_end_parallel_reduce(kID);
}
__attribute__((weak)) void kokkosp_end_parallel_scan(const uint64_t kID) {
  cali::kokkosp_end_parallel_scan(kID);
}
__attribute__((weak)) void kokkosp_end_fence(const uint64_t kID) {
  cali::kokkosp_end_fence(kID);
}

__attribute__((weak)) void kokkosp_push_profile_region(char *regionName) {
  cali::kokkosp_push_profile_region(regionName);
}
__attribute__((weak)) void kokkosp_pop_profile_region() {
  cali::kokkosp_pop_profile_region();
}
__attribute__((weak)) void kokkosp_allocate_data(const SpaceHandle space,
                                      const char *label, const void *const ptr,
                                      const uint64_t size) {
  cali::kokkosp_allocate_data(space, label, ptr, size);
}
__attribute__((weak)) void kokkosp_deallocate_data(const SpaceHandle space,
                                        const char *label,
                                        const void *const ptr,
                                        const uint64_t size) {
  cali::kokkosp_deallocate_data(space, label, ptr, size);
}

__attribute__((weak)) void
kokkosp_begin_deep_copy(const SpaceHandle dst_handle, const char *dst_name,
                        const void *dst_ptr, const SpaceHandle src_space,
                        const char *src_name, const void *src_ptr,
                        const uint64_t size) {
  cali::kokkosp_begin_deep_copy(
      dst_handle, dst_name, dst_ptr, src_space, src_name, src_ptr, size);
}
__attribute__((weak)) void kokkosp_end_deep_copy() {
  cali::kokkosp_end_deep_copy();
}
}
namespace cali {
  extern Kokkos::Tools::Experimental::EventSet get_event_set() {
    Kokkos::Tools::Experimental::EventSet my_event_set;
    my_event_set.init = cali::kokkosp_init_library;
    my_event_set.finalize = cali::kokkosp_finalize_library;
    my_event_set.begin_parallel_for = cali::kokkosp_begin_parallel_for;
    my_event_set.begin_parallel_reduce = cali::kokkosp_begin_parallel_reduce;
    my_event_set.begin_parallel_scan = cali::kokkosp_begin_parallel_scan;
    my_event_set.begin_fence = cali::kokkosp_begin_fence;
    my_event_set.end_fence = cali::kokkosp_end_fence;
    my_event_set.end_parallel_for = cali::kokkosp_end_parallel_for;
    my_event_set.end_parallel_reduce = cali::kokkosp_end_parallel_reduce;
    my_event_set.end_parallel_scan = cali::kokkosp_end_parallel_scan;
    my_event_set.begin_deep_copy = cali::kokkosp_begin_deep_copy;
    my_event_set.end_deep_copy = cali::kokkosp_end_deep_copy;
    my_event_set.parse_args = cali::kokkosp_parse_args;
    my_event_set.print_help = cali::kokkosp_print_help;
    my_event_set.allocate_data = cali::kokkosp_allocate_data;
    my_event_set.deallocate_data = cali::kokkosp_deallocate_data;
    my_event_set.push_region = cali::kokkosp_push_profile_region;
    my_event_set.pop_region = cali::kokkosp_pop_profile_region;
    return my_event_set;
  }  
};