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

#include "caliper/DataTracker.h"

#include "../services/alloc/AllocTracker.h"

#include <map>

using namespace cali;
using namespace DataTracker;

uint64_t g_alloc_id = 0;
AllocTracker g_alloc_tracker;
std::map<uint64_t, TrackedAllocation*> g_tracked_alloc_map;

void* DataTracker::Allocate(const std::string &label,
                            const size_t elem_size,
                            const std::vector<size_t> dimensions)
{
    size_t total_size = 1;
    for(auto d : dimensions)
        total_size *= d;

    void* ret = malloc(total_size*elem_size);

    uint64_t addr = (uint64_t)ret;

    g_tracked_alloc_map[addr] = new TrackedAllocation(label.c_str(), addr, elem_size, dimensions);

    return ret;
}

void DataTracker::Free(void *ptr)
{
    free(ptr);
    delete g_tracked_alloc_map[(uint64_t)ptr];
}

struct TrackedAllocation::Impl {
    const std::string           m_label;
    const uint64_t              m_addr;
    const size_t                m_elem_size;
    const std::vector<size_t>   m_dimensions;

    Impl(const char* label,
         const uint64_t addr,
         const size_t elem_size,
         std::vector<size_t> dimensions)
            : m_label(label),
              m_addr(addr),
              m_elem_size(elem_size),
              m_dimensions(std::move(dimensions))
    {
        size_t total_size = 1;
        for(auto d : dimensions)
            total_size *= d;

        g_alloc_tracker.add_allocation(g_alloc_id++, m_addr, elem_size*total_size);
    }

    ~Impl()
    {
        g_alloc_tracker.remove_allocation(m_addr);
    }

};

TrackedAllocation::TrackedAllocation(const char* label,
                         const uint64_t addr,
                         const size_t elem_size,
                         const std::vector<size_t> &dimensions)
    : pI(new Impl(label, addr, elem_size, dimensions))
{
    g_tracked_alloc_map[addr] = this;
}

TrackedAllocation::~TrackedAllocation()
{
    g_tracked_alloc_map.erase(pI->m_addr);
    delete pI;
}