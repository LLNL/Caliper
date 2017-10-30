// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
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

/// \file cali_datatracker.cpp
/// Caliper data tracking C interface implementation

#include "caliper/cali_datatracker.h"

#include "caliper/DataTracker.h"

using namespace cali;

void*
cali_datatracker_allocate(const char *label,
                          const size_t size) 
{
    return cali::DataTracker::Allocate(label, size);
}

void
cali_datatracker_free(void *ptr) 
{
    cali::DataTracker::Free(ptr);
}

void 
cali_datatracker_track(void *ptr, 
                       const char *label,
                       size_t size) 
{
    cali::DataTracker::TrackAllocation(ptr, label, size);
}

void 
cali_datatracker_track_dimensional(void *ptr, 
                                   const char *label,
                                   size_t elem_size,
                                   const size_t *dimensions,
                                   size_t num_dimensions) 
{
    std::vector<size_t> d(&dimensions[0], &dimensions[0]+num_dimensions);
    cali::DataTracker::TrackAllocation(ptr, label, elem_size, d);
}

void*
cali_datatracker_allocate_dimensional(const char *label,
                                      size_t elem_size, 
                                      const size_t *dimensions, 
                                      size_t num_dimensions)
{
    return cali::DataTracker::Allocate(label, elem_size, std::vector<size_t>(dimensions, dimensions+num_dimensions));
}

void 
cali_datatracker_untrack(void *ptr) 
{
    cali::DataTracker::UntrackAllocation(ptr);
}
