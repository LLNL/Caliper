/* Copyright (c) 2019, Lawrence Livermore National Security, LLC.
 * See top-level LICENSE file for details.
 */

/** 
 * \file cali_datatracker.h 
 * \brief C API for Caliper datatracking.
 */

#ifndef CALI_CALI_DATATRACKER_H
#define CALI_CALI_DATATRACKER_H

#include <stddef.h> /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * --- Data tracking functions ------------------------------------------------
 */

/**
 * \addtogroup AnnotationAPI
 * \{
 */

/**
 * Allocate and track the resulting allocation in Caliper.
 *
 * \note Tracking information will be automatically removed if the allocation is freed
 *
 * \param label The unique label with which to track the allocation requested
 * \param size The total size of the allocation requested
 * \return A new allocation of size `size`
 */

void*
cali_datatracker_allocate(const char *label,
                          size_t size);

/**
 * Allocate and track the resulting allocation, with dimensions, in Caliper.
 *
 * \note Tracking information will be automatically removed if the allocation is freed
 *
 * \param label The unique label with which to track the allocation requested
 * \param elem_size The size of an individual element in the allocation requested
 * \param dimensions An array of the dimensions of the allocation requested
 * \param num_dimensions The number of dimensions in `dimensions`
 * \return A new allocation of size `product(dimensions)*elem_size`
 */

void*
cali_datatracker_allocate_dimensional(const char   *label,
                                      size_t        elem_size, 
                                      const size_t *dimensions, 
                                      size_t        num_dimensions);

/**
 * Free and untrack an allocation in Caliper.
 *
 * \param ptr The pointer to the beginning of the allocation to free/untrack
 */

void
cali_datatracker_free(void *ptr);

/**
 * Track an existing allocation in Caliper.
 *
 * \note This will track the entire allocation pointed to by ptr. 
 *   To track a subset of a larger allocation, use `cali_datatracker_track_dimensional`.
 *
 * \param ptr The pointer to the beginning of the allocation to track
 * \param label The unique label with which to track the allocation 
 * \param size Size of the allocation
 */

void 
cali_datatracker_track(const void *ptr, 
                       const char *label,
                       size_t size);

/**
 * Track an existing allocation in Caliper.
 *
 * \param ptr The pointer to the beginning of the allocation to track
 * \param label The unique label with which to track the allocation 
 * \param elem_size The size of an individual element in the allocation 
 * \param dimensions An array of the dimensions of the allocation 
 * \param num_dimensions The number of dimensions in `dimensions`
 */

void 
cali_datatracker_track_dimensional(const void   *ptr, 
                                   const char   *label,
                                   size_t        elem_size,
                                   const size_t *dimensions,
                                   size_t        num_dimensions);

/**
 * Untrack a previously tracked allocation in Caliper.
 *
 * \param ptr The pointer to the beginning of the allocation to untrack
 */

void 
cali_datatracker_untrack(const void *ptr);

/**
 * \}
 */

#ifdef __cplusplus
} // extern "C"
#endif

/* Include high-level annotation macros. 
 */

#define CALI_DATATRACKER_ALLOCATE(label, elem_size, dimensions, num_dimensions) \
    cali_datatracker_allocate(label, elem_size, dimensions, num_dimensions)

#define CALI_DATATRACKER_FREE(ptr) \
    cali_datatracker_free(ptr)

/**
 * \addtogroup AnnotationAPI
 * \{
 */

/**
 * \brief Label and track the given memory region.
 *
 * Labels the memory region of size \a size bytes pointed to by \a ptr
 * with the variable name of \a ptr. The memory region can then be tracked
 * and resolved with the alloc service.
 */

#define CALI_DATATRACKER_TRACK(ptr, size)            \
    cali_datatracker_track(ptr, #ptr, size)

/**
 * \brief Label and track a multi-dimensional array.
 *
 * Labels a multi-dimensional array with with the variable name of 
 * \a ptr. The array can then be tracked and resolved with the alloc service.
 *
 * \param elem_size Size of the array elements in bytes
 * \param dimension Array of type size_t[] with the sizes of each dimension.
 *    Must have \a num_dimensions entries.
 * \param num_dimensions The number of dimensions
 */

#define CALI_DATATRACKER_TRACK_DIMENSIONAL(ptr, elem_size, dimensions, num_dimensions) \
    cali_datatracker_track_dimensional(ptr, #ptr, elem_size, dimensions, num_dimensions)

/**
 * \brief Stop tracking the memory region pointed to by \a ptr
 */

#define CALI_DATATRACKER_UNTRACK(ptr) \
    cali_datatracker_untrack(ptr)

/**
 * \}
 */

#endif // CALI_CALI_DATATRACKER_H
