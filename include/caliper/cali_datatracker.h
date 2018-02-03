/* *********************************************************************************************
 * Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
 * Produced at the Lawrence Livermore National Laboratory.
 *
 * This file is part of Caliper.
 * Written by Alfredo Gimenez, gimenez1@llnl.gov.
 * LLNL-CODE-678900
 * All rights reserved.
 *
 * For details, see https://github.com/scalability-llnl/Caliper.
 * Please also see the LICENSE file for our additional BSD notice.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the disclaimer below.
 *  * Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions and the disclaimer (as noted below) in the documentation and/or other materials
 *    provided with the distribution.
 *  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * *********************************************************************************************/

/** 
 * \file cali_datatracker.h 
 * \brief C API for Caliper datatracking.
 */

#ifndef CALI_CALI_DATATRACKER_H
#define CALI_CALI_DATATRACKER_H

#include "cali_definitions.h"

#include "common/cali_types.h"
#include "common/cali_variant.h"

#include <stddef.h> /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * --- Data tracking functions ------------------------------------------------
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

#ifdef __cplusplus
} // extern "C"
#endif

/* Include high-level annotation macros. 
 */

#define CALI_DATATRACKER_ALLOCATE(label, elem_size, dimensions, num_dimensions) \
    cali_datatracker_allocate(label, elem_size, dimensions, num_dimensions)

#define CALI_DATATRACKER_FREE(ptr) \
    cali_datatracker_free(ptr)

#define CALI_DATATRACKER_TRACK(ptr, size)            \
    cali_datatracker_track(ptr, #ptr, size)

#define CALI_DATATRACKER_TRACK_DIMENSIONAL(ptr, elem_size, dimensions, num_dimensions) \
    cali_datatracker_track_dimensional(ptr, #ptr, elem_size, dimensions, num_dimensions)

#define CALI_DATATRACKER_UNTRACK(ptr) \
    cali_datatracker_untrack(ptr)

#endif // CALI_CALI_DATATRACKER_H
