/** 
 * @file hail.h 
 * Context annotation library public C interface
 */

#ifndef HAIL_HAIL_H
#define HAIL_HAIL_H

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

ctx_attr_h 
ctx_create_attribute    (const char*         name, 
                         ctx_attr_type       type);


/**
 * Create an attribute with a user-defined datatype
 * @param name Name of the attribute
 * @param typesize Size of the datatype
 * @param properties Attribute properties
 * @return Attribute handle
 */

ctx_attr_h 
ctx_create_usr_attribute(const char*         name,
                         ctx_attr_type       type,
                         size_t              typesize, 
                         ctx_attr_properties properties);

/**
 * Find attribute by name 
 * @param name Name of attribute
 * @return Attribute handle, or CTX_INV_HANDLE if attribute was not found
 */

ctx_attr_h
ctx_find_attribute      (const char*         name);

/**
 * Get name of attribute
 * @param attr Attribute handle
 * @return Attribute name
 */

const char*
ctx_get_attribute_name  (ctx_attr_h          attr);

/**
 * Get attribute datatype and datatype size
 * @param attr Attribute handle
 * @param typesize If not NULL, the call writes the datatype size to this 
 * address
 * @return Attribute name
 */

ctx_attr_type
ctx_get_attribute_type  (ctx_attr_h          attr,
                         size_t*             typesize);


/*
 * --- Environment -----------------------------------------------------
 */

extern "C" {
  typedef ctx_id_t (*cali_environment_callback)(); 
};

/**
 * Get the environment handle of the execution scope (e.g., thread) in which
 * this function is called
 * @return Environment handle
 */

ctx_env_h
ctx_get_environment();

/**
 * Create a new environment for the local scope of execution (e.g., thread) by
 * cloning the current context state of the enclosing environment. (Tool API)
 * @return CTX_SUCCESS if the environment was cloned successfully
 */

ctx_err
ctx_clone_environment(ctx_env_h  env, 
                      ctx_env_h* new_env);

ctx_err
ctx_set_environment_callback(cali_environment_callback cb);


/*
 * --- Context ---------------------------------------------------------
 */

/**
 * 
 *
 */

size_t
ctx_get_context_size (ctx_env_h  env);


/**
 * 
 *
 */

ctx_err
ctx_get_context      (ctx_env_h  env,
                      uint64_t*  buf,
                      size_t     bufsize);



ctx_err
ctx_try_get_context  (ctx_env_h  env,
                      uint64_t*  buf,
                      size_t     bufsize);


/*
 * --- High-level instrumentation API ----------------------------------
 */

/**
 * Begins scope of value @param value for attribute @param attr in environment @env.
 * The new value is nested under the current open scopes of @param attr. 
 */

ctx_err
ctx_begin    (const char* name,
              void*       value);

/*
 * --- Low-level instrumentation API -----------------------------------
 */

/**
 * Begins scope of value @param value for attribute @param attr in environment @env.
 * The new value is nested under the current open scopes of @param attr. 
 */
ctx_err
ctx_begin    (ctx_env_h  env,
              ctx_attr_h attr, 
              void*      value);

/**
 * Ends scope of the innermost value for attribute @param attr in environment @env. 
 */

ctx_err
ctx_end      (ctx_env_h  env,
              ctx_attr_h attr);

/**
 * Ends scope of the current innermost value and begins scope of @param value for attribute
 * @param attr in environment @param env.
 */

ctx_err  
ctx_set      (ctx_env_h  env,
              ctx_attr_h attr, 
              void*      value);

/**
 * Ends scope of the current innermost value and begins scope of @param value for attribute
 * @param attr in environment @param env.
 * May not succeed if concurrent accesses to Caliper data take place. 
 * @return CTX_SUCCESS if successful
 */

ctx_err  
ctx_try_set  (ctx_env_h  env,
              ctx_attr_h attr, 
              void*      value);


/*
 * --- Query / browse API ----------------------------------------------
 */

ctx_entry_t*
ctx_get_entry_for_attribute(const uint64_t* buf,
                            size_t          bufsize,
                            ctx_attr_h      attr,
                            ctx_entry_t*    entry);                        

ctx_entry_t*
ctx_unpack_next     (const uint64_t*    buf,
                     size_t             bufsize,
                     const ctx_entry_t* prev,
                     ctx_entry_t*       entry);

ctx_err
ctx_get_value       (const ctx_entry_t* entry,
                     void*              value);

ctx_attr_h
ctx_get_attribute   (const ctx_entry_t* entry);

ctx_entry_t*
ctx_get_first_child (const ctx_entry_t* from,
                     ctx_entry_t*       child);

ctx_entry_t*
ctx_get_next_sibling(const ctx_entry_t* from,
                     ctx_entry_t*       sibling);

ctx_entry_t*
ctx_get_parent      (const ctx_entry_t* from,
                     ctx_entry_t*       parent);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // HAIL_HAIL_H
