// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

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
