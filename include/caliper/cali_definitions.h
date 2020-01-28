/* Copyright (c) 2019, Lawrence Livermore National Security, LLC.
 * See top-level LICENSE file for details.
 */

/**
 * \file cali_definitions.h
 * \brief Various type definitions for the %Caliper runtime system.
 */

#ifndef CALI_CALI_DEFINITIONS_H
#define CALI_CALI_DEFINITIONS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  CALI_SCOPE_PROCESS = 1,
  CALI_SCOPE_THREAD  = 2,
  CALI_SCOPE_TASK    = 4,
  CALI_SCOPE_CHANNEL = 8
} cali_context_scope_t;

/**
 * Flush options
 */
typedef enum {
    /** Clear trace and aggregation buffers after flush. */
    CALI_FLUSH_CLEAR_BUFFERS = 1
} cali_flush_opt;
  
/**
 * Channel creation options
 */    
typedef enum {
    /** Leave the channel inactive after it is created. */
    CALI_CHANNEL_LEAVE_INACTIVE = 1,
    /** Allow reading configuration entries from environment variables. */
    CALI_CHANNEL_ALLOW_READ_ENV = 2
} cali_channel_opt;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
