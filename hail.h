/** 
 * @file hail.h 
 * Context annotation library public C interface
 */

#ifndef HAIL_HAIL_H
#define HAIL_HAIL_H

#include "hail_types.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * --- Attributes ------------------------------------------------------
 */

ctx_attr_h 
ctx_create_attribute    (const char*         name, 
                         ctx_attr_type       type, 
                         ctx_attr_properties properties);

ctx_attr_h 
ctx_create_usr_attribute(const char*         name, 
                         size_t              typesize, 
                         ctx_attr_properties properties);

ctx_attr_h
ctx_find_attribute      (const char*         name);

const char*
ctx_get_attribute_name  (ctx_attr_h          attr);

ctx_attr_type
ctx_get_attribute_type  (ctx_attr_h          attr,
                         size_t*             typesize);


/*
 * --- Environment -----------------------------------------------------
 */

ctx_env_h
ctx_get_environment();

ctx_err
ctx_clone_environment(ctx_env_h  env, 
                      ctx_env_h* new_env);


/*
 * --- Context ---------------------------------------------------------
 */

size_t
ctx_get_context_size (ctx_env_h  env);

ctx_err
ctx_get_context      (ctx_env_h  env,
                      uint64_t*  buf,
                      size_t     bufsize);

ctx_err
ctx_try_get_context  (ctx_env_h  env,
                      uint64_t*  buf
                      size_t     bufsize);


/*
 * --- Low-level instrumentation API -----------------------------------
 */

ctx_err
ctx_begin    (ctx_env_h  env,
              ctx_attr_h attr, 
              void*      value);

ctx_err
ctx_try_begin(ctx_env_h  env,
              ctx_attr_h attr, 
              void*      value);

ctx_err
ctx_end      (ctx_env_h  env,
              ctx_attr_h attr);

ctx_err
ctx_try_end  (ctx_env_h  env,
              ctx_attr_h attr);

ctx_err  
ctx_set      (ctx_env_h  env,
              ctx_attr_h attr, 
              void*      value);

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
