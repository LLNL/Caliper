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
 * *********************************************************************************************/

/** 
 * \file cali.h 
 * Context annotation library public C interface
 */

#ifndef CALI_CALI_H
#define CALI_CALI_H

#include "cali_definitions.h"

#include "cali_types.h"
#include "cali_variant.h"

#include <stddef.h> /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * --- Attributes ------------------------------------------------------
 */

/**
 * Create an attribute
 * \param name Name of the attribute
 * \param type Type of the attribute
 * \param properties Attribute properties
 * \return Attribute id
 */

cali_id_t 
cali_create_attribute(const char*     name,
                      cali_attr_type  type,
                      int             properties);

/**
 * Create an attribute with additional metadata. 
 * Metadata is provided via (meta-attribute id, pointer-to-data, size) in
 * the \param meta_attr_list, \param meta_val_list, and \param meta_size_list.
 * \param name Name of the attribute
 * \param type Type of the attribute
 * \param properties Attribute properties
 * \param n Number of metadata entries
 * \param meta_attr_list Attribute IDs of the metadata entries
 * \param meta_val_list  Pointers to values of the metadata entries
 * \param meta_size_list Sizes (in bytes) of the metadata values
 * \return Attribute id
 */

cali_id_t
cali_create_attribute_with_metadata(const char*     name,
                                    cali_attr_type  type,
                                    int             properties,
                                    int             n,
                                    const cali_id_t meta_attr_list[],
                                    const void*     meta_val_list[],
                                    const size_t    meta_size_list[]);
  
/**
 * Find attribute by name 
 * \param name Name of attribute
 * \return Attribute ID, or CALI_INV_ID if attribute was not found
 */

cali_id_t
cali_find_attribute  (const char* name);

/**
 * Return name of attribute with given ID
 * \param attr_id Attribute id
 * \return Attribute name, or NULL if `attr_id` is not a valid attribute ID
 */
const char*
cali_attribute_name(cali_id_t attr_id);

/**
 * Return the type of the attribute with given ID
 * \param attr_id Attribute id
 * \return Attribute type, or CALI_TYPE_INV if `attr_id` is not a valid attribute ID
 */
cali_attr_type    
cali_attribute_type(cali_id_t attr_id);

/*
 * --- Snapshot ---------------------------------------------------------
 */

/**
 * Take a snapshot and push it into the processing queue.
 * \param scope Indicates which scopes (process, thread, or task) the 
 *   snapshot should span
 * \param n Number of event info entries
 * \param trigger_info_attr_list Attribute IDs of event info entries
 * \param trigger_info_val_list  Pointers to values of event info entries
 * \param trigger_info_size_list Sizes (in bytes) of event info entries
 */
void
cali_push_snapshot(int scope, int n,
                   const cali_id_t trigger_info_attr_list[],
                   const void*     trigger_info_val_list[],
                   const size_t    trigger_info_size_list[]);

/**
 * Take a snapshot and write it into the user-provided buffer.
 *
 * This function can be safely called from a signal handler. However,
 * it is not guaranteed to succeed. Specifically, the function will
 * fail if the signal handler interrupts already running Caliper
 * code.
 * 
 * The snapshot representation returned in `buf` is valid on the
 * local process only, while Caliper is active (which is up until Caliper's 
 * `finish_evt` callback is invoked).
 *
 * \param scope Indicates which scopes (process, thread, or task) the
 *   snapshot should span
 * \param len   Length of the provided snapshot buffer.
 * \param buf   User-provided snapshot storage buffer.
 * \return Actual size of the snapshot representation. 
 *   If this is larger than `len`, the provided buffer was too small and 
 *   not all of the snapshot was returned.
 *   If this is zero, no snapshot was taken.
 */
size_t
cali_sigsafe_pull_snapshot(int scope, size_t len, unsigned char* buf);

/**
 * Callback function definition for processing a single snapshot entry with 
 * `cali_unpack_snapshot` or `cali_find_all_in_snapshot`. 
 *
 * \param user_arg User-defined argument, passed through by the parent function.
 * \param attr_id The entry's attribute ID
 * \param val The entry's value
 * \return A zero return value tells the parent function to stop processing;
 *   otherwise it will continue.
 */ 
typedef int (*cali_entry_proc_fn)(void* user_arg, cali_id_t attr_id, cali_variant_t val);

/**
 * Unpack a previously obtained snapshot and process its 
 * attribute:value entries with the given `proc_fn` callback function.
 *
 * \note This function is async-signal safe if `proc_fn` is async-signal safe.
 *
 * \param buf Snapshot buffer
 * \param bytes_read Number of bytes read from the buffer
 *   (i.e., length of the snapshot)
 * \param proc_fn Callback function to process individidual entries
 * \param user_arg User-defined parameter passed to `proc_fn`  
 */    
void
cali_unpack_snapshot(const unsigned char* buf,
                     size_t*              bytes_read,
                     cali_entry_proc_fn   proc_fn,
                     void*                user_arg);

cali_variant_t 
cali_find_first_in_snapshot(const unsigned char* buf,
                            size_t*              bytes_read,
                            cali_id_t            attr_id);

void
cali_find_all_in_snapshot(const unsigned char* buf,
                          size_t*              bytes_read,
                          cali_entry_proc_fn   proc_fn,
                          void*                userdata);
    
/*
 * --- Instrumentation API -----------------------------------
 */

/**
 * Put attribute attr on the blackboard. 
 * Parameters:
 * \param attr An attribute of type CALI_TYPE_BOOL
 */

cali_err
cali_begin(cali_id_t   attr);

/**
 * Add \param val for attribute \param attr to the blackboard.
 * The new value is nested under the current value of \param attr. 
 */

cali_err  
cali_begin_double(cali_id_t attr, double val);
cali_err  
cali_begin_int(cali_id_t attr, int val);
cali_err  
cali_begin_string(cali_id_t attr, const char* val);

/**
 * Remove innermost value for attribute `attr` from the blackboard.
 */

cali_err
cali_end  (cali_id_t   attr);

/**
 * Remove innermost value for attribute `attr` from the blackboard.
 * Creates a mismatch warning if the current value does not match `val`.
 * Parameters:
 * \param attr Attribute ID
 * \param val  Expected value
 */

cali_err
cali_safe_end_string(cali_id_t attr, const char* val);

/**
 * Change current innermost value on the blackboard for attribute `attr` 
 * to value taken from `value` with size `size`
 */

cali_err  
cali_set  (cali_id_t   attr, 
           const void* value,
           size_t      size);

cali_err  
cali_set_double(cali_id_t attr, double val);
cali_err  
cali_set_int(cali_id_t attr, int val);
cali_err  
cali_set_string(cali_id_t attr, const char* val);

/**
 * Put attribute with name \param attr_name on the blackboard.
 */

cali_err
cali_begin_byname(const char* attr_name);
  
/**
 * Add \param value for the attribute with the name \param attr_name to the 
 * blackboard.
 */

cali_err
cali_begin_double_byname(const char* attr_name, double val);
cali_err
cali_begin_int_byname(const char* attr_name, int val);
cali_err
cali_begin_string_byname(const char* attr_name, const char* val);

/**
 * Change the value of attribute with the name \param attr_name to \param value 
 * on the blackboard.
 */

cali_err
cali_set_double_byname(const char* attr_name, double val);
cali_err
cali_set_int_byname(const char* attr_name, int val);
cali_err
cali_set_string_byname(const char* attr_name, const char* val);

/**
 * Remove innermost value for attribute \param attr from the blackboard.
 */

cali_err
cali_end_byname(const char* attr_name);

/*
 * --- Runtime system configuration and management
 */

/**
 * Add a value to Caliper's runtime configuration system.
 * Note: only effective _before_ the Caliper runtime system is initialized.
 * \param key   Configuration key (e.g., CALI_SERVICES_ENABLE)
 * \param value Configuration value 
 */
  
void
cali_config_preset(const char* key, const char* value);

/**
 * Initialize Caliper.
 * Typically, it is not necessary to initialize Caliper explicitly.
 * Caliper will lazily initialize itself on the first Caliper API call.
 * This function is used primarily by the Caliper annotation macros,
 * to ensure that Caliper's pre-defined annotation attributes are 
 * initialized.
 * It can also be used to avoid high initialization costs in the first
 * Caliper API call.
 */

void
cali_init();

/*
 * --- Macro annotation helper functions
 */  

/**
 * Create a loop iteration attribute for CALI_MARK_LOOP_BEGIN.
 * \param name User-defined name of the loop
 */
cali_id_t 
cali_make_loop_iteration_attribute(const char* name);

#ifdef __cplusplus
} // extern "C"
#endif

/* Include high-level annotation macros. 
 * In C++, also includes Annotation.h header. 
 */
#include "cali_macros.h"

#endif // CALI_CALI_H
