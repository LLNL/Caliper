// Copyright (c) 2017, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// KokkosLookup.cpp
// Caliper kokkos variable lookup service

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "../../caliper/MemoryPool.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/Annotation.h"

#include <iterator>
#include <cstdint>

#include "types.hpp"

using namespace cali;

namespace {

    class KokkosTime {
        cali::Annotation kernel_name_annot;
        cali::Annotation kernel_type_annot;

        KokkosTime(Caliper *c, Channel *chn)
            : kernel_name_annot("function"),
              kernel_type_annot("kernel_type", CALI_ATTR_SKIP_EVENTS) {
        }

    public:

        void pushRegion(const char *name, const char *type) {
            kernel_type_annot.begin(type);
            kernel_name_annot.begin(name);
        }

        void popRegion() {
            kernel_name_annot.end();
            kernel_type_annot.end();
        }

        static void kokkostime_register(Caliper *c, Channel *chn) {

            auto *instance = new KokkosTime(c, chn);
            chn->events().post_init_evt.connect(
                    [instance](Caliper *c, Channel *chn) {
                        kokkosp_callbacks.kokkosp_begin_parallel_for_callback.connect([&](const char* name, const uint32_t, uint64_t*){
                            instance->pushRegion(name, "kokkos.parallel_for");
                        });
                        kokkosp_callbacks.kokkosp_begin_parallel_reduce_callback.connect([&](const char* name, const uint32_t, uint64_t*){
                            instance->pushRegion(name, "kokkos.parallel_reduce");
                        });
                        kokkosp_callbacks.kokkosp_begin_parallel_scan_callback.connect([&](const char* name, const uint32_t, uint64_t*){
                            instance->pushRegion(name, "kokkos.parallel_scan");
                        });
                        kokkosp_callbacks.kokkosp_begin_fence_callback.connect([&](const char* name, const uint32_t, uint64_t*){
                            instance->pushRegion(name, "kokkos.fence");
                        });
                        kokkosp_callbacks.kokkosp_end_parallel_for_callback.connect([&](const uint64_t){
                            instance->popRegion();
                        });
                        kokkosp_callbacks.kokkosp_end_parallel_reduce_callback.connect([&](const uint64_t){
                            instance->popRegion();
                        });
                        kokkosp_callbacks.kokkosp_end_parallel_scan_callback.connect([&](const uint64_t){
                            instance->popRegion();
                        });
                        kokkosp_callbacks.kokkosp_end_fence_callback.connect([&](const uint64_t){
                            instance->popRegion();
                        });
                        kokkosp_callbacks.kokkosp_push_region_callback.connect([&](const char* regionName){
                            instance->pushRegion(regionName,"kokkos.user_region");
                        });
                        kokkosp_callbacks.kokkosp_pop_region_callback.connect([&](){
                            instance->popRegion();
                        });
                    });
            chn->events().finish_evt.connect(
                [instance](Caliper*, Channel*){
                    delete instance;
                });

            Log(1).stream() << chn->name() << ": Registered kokkostime service" << std::endl;
        }
    };
} // namespace [anonymous]


namespace cali {

    CaliperService kokkostime_service{"kokkostime", ::KokkosTime::kokkostime_register};

}
