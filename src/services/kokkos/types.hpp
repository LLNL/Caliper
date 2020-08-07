#ifndef CALIPER_SERVICES_KOKKOS_COMMON_HPP
#define CALIPER_SERVICES_KOKKOS_COMMON_HPP

#include <cstdint>
#include <caliper/common/util/callback.hpp>


namespace cali {

namespace kokkos {
        struct SpaceHandle {
            char name[64];
        };

        using init_callback = util::callback<void(const int, const uint64_t, const uint32_t, void *)>;
        using finalize_callback = util::callback<void()>;

        using begin_kernel_callback = util::callback<void(const char *, const uint32_t, uint64_t *)>;
        using end_kernel_callback = util::callback<void(const uint64_t)>;

        using push_region_callback = util::callback<void(const char *)>;
        using pop_region_callback = util::callback<void()>;

        using allocation_callback = util::callback<void(const SpaceHandle, const char *, const void *const,
                                                        const uint64_t)>;
        using deallocation_callback = util::callback<void(const SpaceHandle, const char *, const void *const,
                                                          const uint64_t)>;

        using begin_deep_copy_callback = util::callback<void(const SpaceHandle, const char *, const void *,
                                                             const SpaceHandle, const char *, const void *,
                                                             const uint64_t)>;
        using end_deep_copy_callback = util::callback<void()>;
         
	using begin_fence_callback = util::callback<void(const char*, const uint32_t, uint64_t*)>; 
	using end_fence_callback = util::callback<void(uint64_t)>; 
        struct callbacks {

            init_callback kokkosp_init_callback;
            finalize_callback kokkosp_finalize_callback;

            begin_kernel_callback kokkosp_begin_parallel_for_callback;
            end_kernel_callback kokkosp_end_parallel_for_callback;

            begin_kernel_callback kokkosp_begin_parallel_reduce_callback;
            end_kernel_callback kokkosp_end_parallel_reduce_callback;

            begin_kernel_callback kokkosp_begin_parallel_scan_callback;
            end_kernel_callback kokkosp_end_parallel_scan_callback;

            push_region_callback kokkosp_push_region_callback;
            pop_region_callback kokkosp_pop_region_callback;

            allocation_callback kokkosp_allocate_callback;
            deallocation_callback kokkosp_deallocate_callback;

            begin_deep_copy_callback kokkosp_begin_deep_copy_callback;
            end_deep_copy_callback kokkosp_end_deep_copy_callback;

            begin_fence_callback kokkosp_begin_fence_callback;
            end_fence_callback kokkosp_end_fence_callback;
        };

} // end namespace kokkos

} // end namespace caliper

extern cali::kokkos::callbacks kokkosp_callbacks;

#endif
