/** 
 * \file cali.h 
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
 * Find attribute by name 
 * \param name Name of attribute
 * \return Attribute ID, or CALI_INV_ID if attribute was not found
 */

cali_id_t
cali_find_attribute  (const char* name);


/*
 * --- Environment -----------------------------------------------------
 */

/**
 * Get the environment handle of the execution scope \param scope in which
 * this function is called
 * \return Environment handle
 */

void*
cali_current_contextbuffer(cali_context_scope_t scope);

cali_err
cali_create_contextbuffer(cali_context_scope_t scope, void **new_env);


/*
 * --- Context ---------------------------------------------------------
 */

/**
 * Take a snapshot and push it into the processing queue.
 * \param scope Indicates which scopes (process, thread, or task) the 
 *   snapshot should span
 */

void
cali_push_snapshot(int scope);

/*
 * --- Low-level instrumentation API -----------------------------------
 */

/**
 * Begins scope of value \param value of size \param size for 
 * attribute \param attr.
 * The new value is nested under the current open scopes of \param attr. 
 */
cali_err
cali_begin    (cali_id_t   attr, 
               const void* value,
               size_t      size);

/**
 * Ends scope of the innermost value for attribute \param attr.
 */

cali_err
cali_end      (cali_id_t   attr);

/**
 * Ends scope of the current innermost value and begins scope of \param value 
 * of size \param size for attribute \param attr.
 */

cali_err  
cali_set      (cali_id_t   attr, 
               const void* value,
               size_t      size);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CALI_CALI_H
