/** 
 * @file hail.h 
 * Context annotation library public C interface
 */

#ifndef CALI_CALI_H
#define CALI_CALI_H

#include "cali_types.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * --- Attributes ------------------------------------------------------
 */

/**
 * Create an attribute with a predefined datatype
 * @param name Name of the attribute
 * @param type Attribute datatype
 * @param properties Attribute properties
 * @return Attribute handle
 */

cali_attr_h 
cali_create_attribute    (const char*         name, 
                         cali_attr_type       type);


/**
 * Create an attribute with a user-defined datatype
 * @param name Name of the attribute
 * @param typesize Size of the datatype
 * @param properties Attribute properties
 * @return Attribute handle
 */

cali_attr_h 
cali_create_usr_attribute(const char*         name,
                         cali_attr_type       type,
                         size_t              typesize, 
                         cali_attr_properties properties);

/**
 * Find attribute by name 
 * @param name Name of attribute
 * @return Attribute handle, or CALI_INV_HANDLE if attribute was not found
 */

cali_attr_h
cali_find_attribute      (const char*         name);

/**
 * Get name of attribute
 * @param attr Attribute handle
 * @return Attribute name
 */

const char*
cali_get_attribute_name  (cali_attr_h          attr);

/**
 * Get attribute datatype and datatype size
 * @param attr Attribute handle
 * @param typesize If not NULL, the call writes the datatype size to this 
 * address
 * @return Attribute name
 */

cali_attr_type
cali_get_attribute_type  (cali_attr_h          attr,
                         size_t*             typesize);


/*
 * --- Environment -----------------------------------------------------
 */

typedef cali_id_t (*cali_environment_callback)(); 

/**
 * Get the environment handle of the execution scope (e.g., thread) in which
 * this function is called
 * @return Environment handle
 */

cali_env_h
cali_get_environment();

/**
 * Create a new environment for the local scope of execution (e.g., thread) by
 * cloning the current context state of the enclosing environment. (Tool API)
 * @return CALI_SUCCESS if the environment was cloned successfully
 */

cali_err
cali_clone_environment(cali_env_h  env, 
                      cali_env_h* new_env);

cali_err
cali_set_environment_callback(cali_environment_callback cb);


/*
 * --- Context ---------------------------------------------------------
 */

/**
 * 
 *
 */

size_t
cali_get_context_size (cali_env_h  env);


/**
 * 
 *
 */

cali_err
cali_get_context      (cali_env_h  env,
                      uint64_t*  buf,
                      size_t     bufsize);

cali_err
cali_try_get_context  (cali_env_h  env,
                      uint64_t*  buf,
                      size_t     bufsize);

/*
 * --- Low-level instrumentation API -----------------------------------
 */

/**
 * Begins scope of value @param value for attribute @param attr in environment @env.
 * The new value is nested under the current open scopes of @param attr. 
 */
cali_err
cali_begin    (cali_env_h  env,
              cali_attr_h attr, 
              void*      value);

/**
 * Ends scope of the innermost value for attribute @param attr in environment @env. 
 */

cali_err
cali_end      (cali_env_h  env,
              cali_attr_h attr);

/**
 * Ends scope of the current innermost value and begins scope of @param value for attribute
 * @param attr in environment @param env.
 */

cali_err  
cali_set      (cali_env_h  env,
              cali_attr_h attr, 
              void*      value);

/**
 * Ends scope of the current innermost value and begins scope of @param value for attribute
 * @param attr in environment @param env.
 * May not succeed if concurrent accesses to Caliper data take place. 
 * @return CALI_SUCCESS if successful
 */

cali_err  
cali_try_set  (cali_env_h  env,
              cali_attr_h attr, 
              void*      value);


/*
 * --- Query / browse API ----------------------------------------------
 */

cali_entry_t*
cali_get_entry_for_attribute(const uint64_t* buf,
                            size_t          bufsize,
                            cali_attr_h      attr,
                            cali_entry_t*    entry);                        

cali_entry_t*
cali_unpack_next     (const uint64_t*    buf,
                     size_t             bufsize,
                     const cali_entry_t* prev,
                     cali_entry_t*       entry);

cali_err
cali_get_value       (const cali_entry_t* entry,
                     void*              value);

cali_attr_h
cali_get_attribute   (const cali_entry_t* entry);

cali_entry_t*
cali_get_first_child (const cali_entry_t* from,
                     cali_entry_t*       child);

cali_entry_t*
cali_get_next_sibling(const cali_entry_t* from,
                     cali_entry_t*       sibling);

cali_entry_t*
cali_get_parent      (const cali_entry_t* from,
                     cali_entry_t*       parent);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // CALI_CALI_H
