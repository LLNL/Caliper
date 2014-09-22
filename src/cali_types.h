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
  CTX_TYPE_USR = 0,
  CTX_TYPE_INT,
  CTX_TYPE_STRING,
  CTX_TYPE_ADDR,
  CTX_TYPE_DOUBLE,
} ctx_attr_type;

typedef enum {
  CTX_ATTR_BASIC       = 0,
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

#endif // CALI_CALI_TYPES_H
