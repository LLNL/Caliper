// Copyright (c) 2018, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by Alfredo Gimenez, gimenez1@llnl.gov
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

// Caliper data tracking C interface implementation

#include "caliper/cali_datatracker.h"
#include "caliper/Caliper.h"

#include <cstdlib>
#include <numeric>

using namespace cali;


void*
cali_datatracker_allocate(const char *label,
                          const size_t size) 
{
    void* ptr = malloc(size);
    cali_datatracker_track(ptr, label, size);

    return ptr;
}

void
cali_datatracker_free(void *ptr) 
{
    cali_datatracker_untrack(ptr);
    free(ptr);
}

void 
cali_datatracker_track(const void *ptr, 
                       const char *label,
                       size_t size) 
{
    Caliper::instance().memory_region_begin(ptr, label, 1, 1, &size);
}

void 
cali_datatracker_track_dimensional(const void      *ptr, 
                                   const char      *label,
                                   size_t           elem_size,
                                   const size_t    *dimensions,
                                   size_t           ndims) 
{
    Caliper::instance().memory_region_begin(ptr, label, elem_size, ndims, dimensions);
}

void*
cali_datatracker_allocate_dimensional(const char   *label,
                                      size_t        elem_size, 
                                      const size_t *dimensions, 
                                      size_t        ndims)
{
    void* ptr =
        malloc( std::accumulate(dimensions, dimensions+ndims, elem_size,
                                std::multiplies<size_t>()) );

    cali_datatracker_track_dimensional(ptr, label, elem_size, dimensions, ndims);

    return ptr;
}

void 
cali_datatracker_untrack(const void *ptr) 
{
    Caliper::instance().memory_region_end(ptr);
}
