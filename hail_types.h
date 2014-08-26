/** 
 * @file hail_types.h 
 * Context annotation library typedefs
 */

#ifndef CTX_HAIL_TYPES_H
#define CTX_HAIL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t ctx_id_t;

typedef struct _ctx_entry {
  ctx_attr_h    attr;
  union {
    uint64_t    value;
    ctx_node_t* node;
  }
} ctx_entry_t;

typedef enum {
  CTX_TYPE_INT,
  CTX_TYPE_STRING16,
  CTX_TYPE_STRING256,
  CTX_TYPE_ADDR,
  CTX_TYPE_USR
} ctx_attr_type;

typedef enum {
  CTX_ATTR_BYVALUE     = (1 << 0),
  CTX_ATTR_AUTOCOMBINE = (1 << 1),
  CTX_ATTR_NOCLONE     = (1 << 2)
} ctx_attr_properties;

typedef enum {
  CTX_SUCCESS = 0,
  CTX_EBUSY,
  CTX_ELOCKED,
  CTX_EINV
} ctx_err;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CTX_HAIL_TYPES_H
