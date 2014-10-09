/** 
 * @file cali_types.h 
 * Context annotation library typedefs
 */

#ifndef CALI_CALI_TYPES_H
#define CALI_CALI_TYPES_H

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t ctx_id_t;

#define CTX_INV_ID     0xFFFFFFFFFFFFFFFF
#define CTX_INV_HANDLE 0

/* typedef struct _ctx_entry { */
/*   ctx_id_t      attr; */
/*   union { */
/*     int64_t     value; */
/*     ctx_node_h  node; */
/*   } */
/* } ctx_entry_t; */

typedef enum {
  CTX_TYPE_INV    = -1,
  CTX_TYPE_USR    = 0,
  CTX_TYPE_INT    = 1,
  CTX_TYPE_STRING = 2,
  CTX_TYPE_ADDR   = 3,
  CTX_TYPE_DOUBLE = 4
} ctx_attr_type;

typedef enum {
  CTX_ATTR_DEFAULT     = 0,
  CTX_ATTR_ASVALUE     = 1,
  CTX_ATTR_NOMERGE     = 2,
  CTX_ATTR_GLOBAL      = 4
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

#endif // CALI_CALI_TYPES_H
