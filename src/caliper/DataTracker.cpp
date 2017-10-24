// Copyright (c) 2015, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by Alfredo Gimenez, gimenez1@llnl.gov.
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

#include "caliper/AllocTracker.h"
#include "caliper/DataTracker.h"

#include <iostream>

// malloc_usable_size() is Linux-specific
// #include <malloc.h>

namespace cali
{
namespace DataTracker
{

AllocTracker g_alloc_tracker;

void* Allocate(const std::string &label,
               const size_t size) {

    void* ret = malloc(size);

    size_t dimensions[] = {size};
    g_alloc_tracker.add_allocation(label, (uint64_t)ret, 1, dimensions, 1);

    return ret;
}

void* Allocate(const std::string &label,
               const size_t elem_size,
               const std::vector<size_t> &dimensions) {

    // FIXME: the num_bytes calculation happens twice here...
    size_t total_size = Allocation::num_bytes(elem_size, (size_t*)dimensions.data(), (size_t)dimensions.size());

    void* ret = malloc(total_size);

    g_alloc_tracker.add_allocation(label, (uint64_t)ret, elem_size, (size_t*)dimensions.data(), (size_t)dimensions.size());

    return ret;
}

void TrackAllocation(void *ptr,
                     const std::string &label,
                     size_t size) {

    // size_t size[] = {malloc_usable_size(ptr)};
    // if (size[0] > 0)
    g_alloc_tracker.add_allocation(label, (uint64_t)ptr, (size_t)1, &size, (size_t)1);
    // else
    //     std::cerr << "Invalid allocation to track!" << std::endl;
}

void TrackAllocation(void *ptr,
                     const std::string &label,
                     const size_t elem_size,
                     const std::vector<size_t> &dimensions) {
    // size_t size = malloc_usable_size(ptr);
    // if (size >= Allocation::num_bytes(elem_size, (size_t*)dimensions.data(), (size_t)dimensions.size()))
    g_alloc_tracker.add_allocation(label, (uint64_t)ptr, elem_size, dimensions.data(), (size_t)dimensions.size());
    // else
    //     std::cerr << "Invalid allocation tracking information!" << std::endl;
}

void UntrackAllocation(void *ptr) {
    g_alloc_tracker.remove_allocation((uint64_t)ptr);
}

void Free(void *ptr)
{
    free(ptr);
}

}
}
