#include "caliper/Annotation.h"
#include "caliper/ConfigManager.h"
#include "caliper/cali.h"
#include "types.hpp"
#include <cstdint>
#include <impl/Kokkos_Profiling_Interface.hpp>
cali::kokkos::callbacks kokkosp_callbacks;
using cali::kokkos::SpaceHandle;
namespace kokkos {
cali::ConfigManager mgr;
bool can_control_fences = false;
bool config_needs_fences = true;
Kokkos::Tools::Experimental::ToolProgrammingInterface kokkos_interface;
template <typename Container, typename FenceIf>
void fence_if_needed(const Container &in, uint32_t dev_id,
                     const FenceIf &predicate) {
  if (can_control_fences && !in.empty() && predicate()) {
    kokkos_interface.fence(dev_id);
  }
}
} // namespace kokkos
extern "C" void kokkosp_print_help(char *progName) {
  std::cerr << "Caliper: available configs: \n";
  for (auto conf : kokkos::mgr.available_config_specs()) {
    std::cerr << kokkos::mgr.get_documentation_for_spec(conf.c_str())
              << std::endl;
  }
}
extern "C" void kokkosp_request_tool_settings(
    int num_actions, Kokkos::Tools::Experimental::ToolSettings *settings) {
  if ((num_actions > 0) && (settings != nullptr)) {
    settings->requires_global_fencing = false;
  }
}

extern "C" void kokkosp_provide_tool_programming_interface(
    int num_actions,
    Kokkos::Tools::Experimental::ToolProgrammingInterface interface) {
  kokkos::can_control_fences = true;
  kokkos::kokkos_interface = interface;
}
extern "C" void kokkosp_parse_args(int argc, char *argv_raw[]) {
  if (argc > 2) {
    std::cerr << "Error: the Kokkos Caliper connector takes only one argument"
              << std::endl;
  }
  if (argc == 2) {
    if (std::string(argv_raw[1]).find("cuda-activity-report") !=
        std::string::npos) {
      kokkos::config_needs_fences = false;
    }
    kokkos::mgr.add(argv_raw[1]);
    if (kokkos::mgr.error()) {
      std::cerr << "Kokkos Caliper connector error: " << kokkos::mgr.error_msg()
                << std::endl;
    }
    kokkos::mgr.start();
  }
}

extern "C" void kokkosp_init_library(const int loadSeq,
                                     const uint64_t interfaceVer,
                                     const uint32_t devInfoCount,
                                     void *deviceInfo) {
  cali_init();
  kokkosp_callbacks.kokkosp_init_callback(loadSeq, interfaceVer, devInfoCount,
                                          deviceInfo);
}

extern "C" void kokkosp_finalize_library() {
  kokkos::mgr.flush();
  kokkosp_callbacks.kokkosp_finalize_callback();
}

extern "C" void kokkosp_begin_parallel_for(const char *name,
                                           const uint32_t devID,
                                           uint64_t *kID) {
  kokkos::fence_if_needed(kokkosp_callbacks.kokkosp_begin_parallel_for_callback,
                          devID, [=]() { return kokkos::config_needs_fences; });
  kokkosp_callbacks.kokkosp_begin_parallel_for_callback(name, devID, kID);
  *kID = devID;
}
extern "C" void kokkosp_begin_parallel_reduce(const char *name,
                                              const uint32_t devID,
                                              uint64_t *kID) {
  kokkos::fence_if_needed(
      kokkosp_callbacks.kokkosp_begin_parallel_reduce_callback, devID,
      [=]() { return kokkos::config_needs_fences; });
  kokkosp_callbacks.kokkosp_begin_parallel_reduce_callback(name, devID, kID);
  *kID = devID;
}
extern "C" void kokkosp_begin_parallel_scan(const char *name,
                                            const uint32_t devID,
                                            uint64_t *kID) {
  kokkos::fence_if_needed(
      kokkosp_callbacks.kokkosp_begin_parallel_scan_callback, devID,
      [=]() { return kokkos::config_needs_fences; });
  kokkosp_callbacks.kokkosp_begin_parallel_scan_callback(name, devID, kID);
  *kID = devID;
}
extern "C" void kokkosp_begin_fence(const char *name, const uint32_t devID,
                                    uint64_t *kID) {
  kokkosp_callbacks.kokkosp_begin_fence_callback(name, devID, kID);
}

extern "C" void kokkosp_end_parallel_for(const uint64_t kID) {
  kokkos::fence_if_needed(kokkosp_callbacks.kokkosp_end_parallel_for_callback,
                          kID, [=]() { return kokkos::config_needs_fences; });
  kokkosp_callbacks.kokkosp_end_parallel_for_callback(kID);
}
extern "C" void kokkosp_end_parallel_reduce(const uint64_t kID) {
  kokkos::fence_if_needed(
      kokkosp_callbacks.kokkosp_end_parallel_reduce_callback, kID,
      [=]() { return kokkos::config_needs_fences; });
  kokkosp_callbacks.kokkosp_end_parallel_reduce_callback(kID);
}
extern "C" void kokkosp_end_parallel_scan(const uint64_t kID) {
  kokkos::fence_if_needed(kokkosp_callbacks.kokkosp_end_parallel_scan_callback,
                          kID, [=]() { return kokkos::config_needs_fences; });
  kokkosp_callbacks.kokkosp_end_parallel_scan_callback(kID);
}
extern "C" void kokkosp_end_fence(const uint64_t kID) {
  kokkosp_callbacks.kokkosp_end_fence_callback(kID);
}

extern "C" void kokkosp_push_profile_region(char *regionName) {
  kokkos::fence_if_needed(kokkosp_callbacks.kokkosp_push_region_callback, 0,
                          [=]() { return true; });
  kokkosp_callbacks.kokkosp_push_region_callback(regionName);
}
extern "C" void kokkosp_pop_profile_region() {
  kokkos::fence_if_needed(kokkosp_callbacks.kokkosp_push_region_callback, 0,
                          [=]() { return true; });
  kokkosp_callbacks.kokkosp_pop_region_callback();
}
extern "C" void kokkosp_allocate_data(const SpaceHandle space,
                                      const char *label, const void *const ptr,
                                      const uint64_t size) {
  kokkosp_callbacks.kokkosp_allocate_callback(space, label, ptr, size);
}
extern "C" void kokkosp_deallocate_data(const SpaceHandle space,
                                        const char *label,
                                        const void *const ptr,
                                        const uint64_t size) {
  kokkosp_callbacks.kokkosp_deallocate_callback(space, label, ptr, size);
}

extern "C" void
kokkosp_begin_deep_copy(const SpaceHandle dst_handle, const char *dst_name,
                        const void *dst_ptr, const SpaceHandle src_space,
                        const char *src_name, const void *src_ptr,
                        const uint64_t size) {
  kokkosp_callbacks.kokkosp_begin_deep_copy_callback(
      dst_handle, dst_name, dst_ptr, src_space, src_name, src_ptr, size);
}
extern "C" void kokkosp_end_deep_copy() {
  kokkosp_callbacks.kokkosp_end_deep_copy_callback();
}
