/** 
 * @file cali.h 
 * Context annotation library public C interface
 */

#ifndef CALI_CALI_H
#define CALI_CALI_H

#include "cali_definitions.h"

#include <cali_types.h>

#include <stddef.h> /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * --- Attributes ------------------------------------------------------
 */

/**
 * Create an attribute
 * @param name Name of the attribute
 * @param type Type of the attribute
 * @param properties Attribute properties
 * @return Attribute id
 */

cali_id_t 
cali_create_attribute(const char*          name,
                      cali_attr_type       type,
                      cali_attr_properties properties);

/**
 * Find attribute by name 
 * @param name Name of attribute
 * @return Attribute ID, or CALI_INV_ID if attribute was not found
 */

cali_id_t
cali_find_attribute      (const char*         name);

/**
 * Get name of attribute
 * @param attr Attribute id
 * @return Attribute name
 */

const char*
cali_get_attribute_name  (cali_id_t          attr);

/**
 * Get attribute datatype and datatype size
 * @param attr Attribute id
 * @param typesize If not NULL, the call writes the datatype size to this 
 * address
 * @return Attribute name
 */

cali_attr_type
cali_get_attribute_type  (cali_id_t          attr,
                          size_t*            typesize);


/*
 * --- Environment -----------------------------------------------------
 */

typedef cali_id_t (*cali_environment_callback)(); 

/**
 * Get the environment handle of the execution scope @param scope in which
 * this function is called
 * @return Environment handle
 */

cali_id_t
cali_current_environment(cali_context_scope_t scope);

cali_err
cali_create_environment(cali_id_t *new_env);

/**
 * Create a new environment by
 * cloning the current context state of the enclosing environment. (Tool API)
 * @return CALI_SUCCESS if the environment was cloned successfully
 */

cali_err
cali_clone_environment(cali_id_t  env, 
                       cali_id_t* new_env);

cali_err
cali_release_environment(cali_id_t env);

cali_err
cali_set_environment_callback(cali_context_scope_t scope, cali_environment_callback cb);


/*
 * --- Context ---------------------------------------------------------
 */

/**
 * 
 * 
 */

size_t
cali_get_context_size(cali_context_scope_t scope);

/**
 * 
 *
 */

size_t
cali_get_context     (cali_context_scope_t scope,
                      uint64_t*            buf,
                      size_t               bufsize);

size_t
cali_try_get_context (cali_context_scope_t scope,
                      uint64_t*            buf,
                      size_t               bufsize);

/*
 * --- Low-level instrumentation API -----------------------------------
 */

/**
 * Begins scope of value @param value of size @param size for 
 * attribute @param attr.
 * The new value is nested under the current open scopes of @param attr. 
 */
cali_err
cali_begin    (cali_id_t   attr, 
               const void* value,
               size_t      size);

/**
 * Ends scope of the innermost value for attribute @param attr.
 */

cali_err
cali_end      (cali_id_t  attr);

/**
 * Ends scope of the current innermost value and begins scope of @param value 
 * of size @param size for attribute @param attr.
 */

cali_err  
cali_set      (cali_id_t   attr, 
               const void* value,
               size_t      size);

/**
 * Ends scope of the current innermost value and begins scope of @param value for attribute
 * @param attr.
 * May not succeed if concurrent accesses to Caliper data take place. 
 * @return CALI_SUCCESS if successful
 */

cali_err  
cali_try_set  (cali_id_t   attr,
               const void* value);

/**
 * Write metadata using the current run-time configuration settings
 */

cali_err
cali_write_metadata(void);

/*
 * --- Query / browse API ----------------------------------------------
 */

/*

cali_entry_t*
cali_get_entry_for_attribute(const uint64_t* buf,
                            size_t          bufsize,
                            cali_id_t      attr,
                            cali_entry_t*    entry);                        

cali_entry_t*
cali_unpack_next    (const uint64_t*    buf,
                     size_t             bufsize,
                     const cali_entry_t* prev,
                     cali_entry_t*       entry);

cali_err
cali_get_value      (const cali_entry_t* entry,
                     void*              value);

cali_id_t
cali_get_attribute   (const cali_entry_t* entry);

cali_entry_t*
cali_get_first_child(const cali_entry_t* from,
                     cali_entry_t*       child);

cali_entry_t*
cali_get_next_sibling(const cali_entry_t* from,
                      cali_entry_t*       sibling);

cali_entry_t*
cali_get_parent      (const cali_entry_t* from,
                      cali_entry_t*       parent);
*/

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CALI_CALI_H
