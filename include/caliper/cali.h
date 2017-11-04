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
 * \brief C API for Caliper.
 */

#ifndef CALI_CALI_H
#define CALI_CALI_H

#include "cali_definitions.h"

#include "common/cali_types.h"
#include "common/cali_variant.h"

#include <stddef.h> /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * --- Type definitions ------------------------------------------------
 */

/**
 * \brief Callback function to process a snapshot entry.
 *
 * Callback function definition for processing a single snapshot entry with 
 * cali_unpack_snapshot() or cali_find_all_in_snapshot(). 
 *
 * \param user_arg User-defined argument, passed through by the parent function.
 * \param attr_id The entry's attribute ID
 * \param val The entry's value
 * \return A zero return value tells the parent function to stop processing;
 *   otherwise it will continue.
 */ 
typedef int (*cali_entry_proc_fn)(void* user_arg, cali_id_t attr_id, cali_variant_t val);

/*
 * --- Attributes ------------------------------------------------------
 */

/**
 * \name Attribute management
 * \{
 */

/**
 * \brief Create an attribute
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
 * \brief Create an attribute with additional metadata. 
 *
 * Metadata is provided via (meta-attribute id, pointer-to-data, size) in
 * the \a meta_attr_list, \a meta_val_list, and \a meta_size_list.
 * \param name Name of the attribute
 * \param type Type of the attribute
 * \param properties Attribute properties
 * \param n Number of metadata entries
 * \param meta_attr_list Attribute IDs of the metadata entries
 * \param meta_val_list  Pointers to values of the metadata entries
 * \param meta_size_list Sizes (in bytes) of the metadata values
 * \return Attribute id
 * \sa cali_create_attribute
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
 * \brief Find attribute by name 
 * \param name Name of attribute
 * \return Attribute ID, or CALI_INV_ID if attribute was not found
 */

cali_id_t
cali_find_attribute  (const char* name);

/**
 * \brief  Return name of attribute with given ID
 * \param  attr_id Attribute id
 * \return Attribute name, or NULL if `attr_id` is not a valid attribute ID
 */
const char*
cali_attribute_name(cali_id_t attr_id);

/**
 * \brief Return the type of the attribute with given ID
 * \param attr_id Attribute id
 * \return Attribute type, or CALI_TYPE_INV if `attr_id` is not a valid attribute ID
 */
cali_attr_type    
cali_attribute_type(cali_id_t attr_id);

/**
 * \} 
 */

/*
 * --- Snapshot ---------------------------------------------------------
 */

/**
 * \name Taking snapshots
 * \{
 */

/**
 * \brief Take a snapshot and push it into the processing queue.
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
 * \brief Take a snapshot and write it into the user-provided buffer.
 *
 * This function can be safely called from a signal handler. However,
 * it is not guaranteed to succeed. Specifically, the function will
 * fail if the signal handler interrupts already running Caliper
 * code.
 * 
 * The snapshot representation returned in \a buf is valid only on the
 * local process, while Caliper is active (which is up until Caliper's 
 * `finish_evt` callback is invoked).
 * It can be parsed with cali_unpack_snapshot().
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
cali_pull_snapshot(int scope, size_t len, unsigned char* buf);

/**
 * \}
 * \name Processing snapshot contents
 * \{
 */

/**
 * \brief Unpack a snapshot buffer.
 *
 * Unpack a snapshot that was previously obtained on the same process
 * and examine its attribute:value entries with the given \a proc_fn 
 * callback function.
 *
 * The function will invoke \a proc_fn repeatedly, once for each
 * unpacked entry. \a proc_fn should return a non-zero value if it
 * wants to continue processing, otherwise processing will stop. Note
 * that snapshot processing cannot be re-started from a partially read
 * snapshot buffer position: the buffer has to be read again from the
 * beginning.
 *
 * Hierarchical values will be given to \a proc_fn in top-down order.
 *
 * \note This function is async-signal safe if \a proc_fn is
 *   async-signal safe.
 *
 * \param buf Snapshot buffer
 * \param bytes_read Number of bytes read from the buffer
 *   (i.e., length of the snapshot)
 * \param proc_fn Callback function to process individidual entries
 * \param user_arg User-defined parameter passed through to \a proc_fn
 *
 * \sa cali_pull_snapshot, cali_entry_proc_fn
 */    
void
cali_unpack_snapshot(const unsigned char* buf,
                     size_t*              bytes_read,
                     cali_entry_proc_fn   proc_fn,
                     void*                user_arg);

/**
 * Return top-most value for attribute ID \a attr_id from snapshot \a buf.
 * The snapshot must have previously been obtained on the same process with
 * cali_pull_snapshot().
 *
 * \note This function is async-signal safe
 *
 * \param buf Snapshot buffer
 * \param attr_id Attribute id
 * \param bytes_read Number of bytes read from the buffer
 *   (i.e., length of the snapshot)
 * \return The top-most stacked value for the given attribute ID, or an empty
 *   variant if none was found
 */    

cali_variant_t 
cali_find_first_in_snapshot(const unsigned char* buf,
                            cali_id_t            attr_id,
                            size_t*              bytes_read);

/**
 * Run all entries with attribute `attr_id` in a snapshot that was previously 
 * obtained on the same process through the given `proc_fn` callback function.
 *
 * \note This function is async-signal safe if `proc_fn` is async-signal safe.
 *
 * \param buf Snapshot buffer
 * \param attr_id Attribute to read from snapshot
 * \param bytes_read Number of bytes read from the buffer
 *   (i.e., length of the snapshot)
 * \param proc_fn Callback function to process individidual entries
 * \param userdata User-defined parameter passed to `proc_fn`  
 */    

void
cali_find_all_in_snapshot(const unsigned char* buf,
                          cali_id_t            attr_id,
                          size_t*              bytes_read,
                          cali_entry_proc_fn   proc_fn,
                          void*                userdata);

/**
 * \}
 */

/*
 * --- Blackboard access API ---------------------------------
 */

/**
 * \name Blackboard access
 * \{
 */

/**
 * \brief Return top-most value for attribute \a attr_id from the blackboard.
 *
 * \note This function is async-signal safe.
 *
 * \param attr_id Attribute ID to find
 * \return The top-most stacked value on the blackboard for the given
 *    attribute ID, or an empty variant if it was not found
 */
cali_variant_t
cali_get(cali_id_t attr_id);

/**
 * \}
 */
    
/*
 * --- Instrumentation API -----------------------------------
 */

/**
 * \addtogroup AnnotationAPI
 * \{
 * \name Low-level source-code annotation API
 * \{
 */

/**
 * \brief Put attribute attr on the blackboard. 
 * Parameters:
 * \param attr An attribute of type CALI_TYPE_BOOL
 */

cali_err
cali_begin(cali_id_t   attr);

/**
 * Add \a val for attribute \a attr to the blackboard.
 * The new value is nested under the current value of \a attr. 
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
 * \brief Remove innermost value for attribute \a attr from the blackboard.
 *
 * Creates a mismatch warning if the current value does not match \a val.
 * This function is primarily used by the high-level annotation API.
 *
 * \param attr Attribute ID
 * \param val  Expected value
 */

cali_err
cali_safe_end_string(cali_id_t attr, const char* val);

/**
 * \brief Change current innermost value on the blackboard for attribute \a attr 
 * to value taken from \a value with size \a size
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
 * Put attribute with name \a attr_name on the blackboard.
 */

cali_err
cali_begin_byname(const char* attr_name);
  
/**
 * \brief Add \a value for the attribute with the name \a attr_name to the 
 * blackboard.
 */

cali_err
cali_begin_double_byname(const char* attr_name, double val);
cali_err
cali_begin_int_byname(const char* attr_name, int val);
cali_err
cali_begin_string_byname(const char* attr_name, const char* val);

/**
 * \brief Change the value of attribute with the name \a attr_name to \a value 
 * on the blackboard.
 */

cali_err
cali_set_double_byname(const char* attr_name, double val);
cali_err
cali_set_int_byname(const char* attr_name, int val);
cali_err
cali_set_string_byname(const char* attr_name, const char* val);

/**
 * \brief Remove innermost value for attribute \a attr from the blackboard.
 */

cali_err
cali_end_byname(const char* attr_name);

/**
 * \}
 * \}
 */

/*
 * --- Runtime system configuration
 */
    
/**
 * \name Runtime configuration
 * \{
 */
    
/**
 * \copydoc cali::RuntimeConfig::preset
 */
  
void
cali_config_preset(const char* key, const char* value);

/**
 * \copydoc cali::RuntimeConfig::set
 */
  
void
cali_config_set(const char* key, const char* value);

/**
 * \copybrief cali::RuntimeConfig::define_profile
 *
 * A configuration profile is a named set of specific
 * configuration settings. The entire set can be enabled by its name
 * with a single configuration entry.
 *
 * This function only defines a configuration profile, but does not
 * enable it. %Caliper uses the profiles named in the
 * \t CALI_CONFIG_PROFILE configuration entry; to enable a profile
 * set this configuration entry accordingly.
 *
 * Example:
 *
 * \code
 *   const char* my_profile[][2] =
 *     { { "CALI_SERVICES_ENABLE", "aggregate,event,timestamp,trace" },
 *       { "CALI_EVENT_TRIGGER",   "annotation" },
 *       { NULL, NULL }
 *     };
 *
 *   // Define the "my_profile" config profile
 *   cali_config_define_profile("my_profile", my_profile);
 *   cali_config_set("CALI_CONFIG_PROFILE", "my_profile");
 * \endcode
 *
 * \param name Name of the configuration profile.
 * \param keyvallist A list of key-value pairs as array of two strings
 *    that contains the profile's configuration entries. The first string
 *    in each entry is the configuration key, the second string is its
 *    value. Keys must be all uppercase. Terminate the list with two
 *    NULL entries: <tt> { NULL, NULL } </tt>.
 */
  
void
cali_config_define_profile(const char* name, const char* keyvallist[][2]);

/**
 * \copydoc cali::RuntimeConfig::allow_read_env(bool)
 */
  
void
cali_config_allow_read_env(int allow);

/**
 * \}
 */
    
/*
 * --- Runtime system management
 */

/**
 * \brief Initialize Caliper.
 *
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

/**
 * \brief  Check if Caliper is initialized on this process.
 * \return A non-zero value if Caliper is initialized, 0 if it is not initialized.
 */

int
cali_is_initialized();
    
/*
 * --- Macro annotation helper functions
 */  

/**
 * \brief Create a loop iteration attribute for CALI_MARK_LOOP_BEGIN.
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
