// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Caliper data tracking C interface implementation

#include "caliper/cali_datatracker.h"
#include "caliper/Caliper.h"

#include <cstdlib>
#include <numeric>

using namespace cali;

void cali_datatracker_track(const void* ptr, const char* label, size_t size)
{
    Caliper::instance().memory_region_begin(ptr, label, 1, 1, &size);
}

void cali_datatracker_track_dimensional(
    const void*   ptr,
    const char*   label,
    size_t        elem_size,
    const size_t* dimensions,
    size_t        ndims
)
{
    Caliper::instance().memory_region_begin(ptr, label, elem_size, ndims, dimensions);
}

void cali_datatracker_untrack(const void* ptr)
{
    Caliper::instance().memory_region_end(ptr);
}
