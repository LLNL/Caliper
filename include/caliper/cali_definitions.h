/* *********************************************************************************************
 * Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
 * Produced at the Lawrence Livermore National Laboratory.
 *
 * This file is part of Caliper.
 * Written by David Boehme, boehme3@llnl.gov.
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
 * ********************************************************************************************/

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
  CALI_SCOPE_TASK    = 4 
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
