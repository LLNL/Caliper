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

typedef uint64_t cali_id_t;

#define CALI_INV_ID     0xFFFFFFFFFFFFFFFF

typedef enum {
  CALI_TYPE_INV    = -1,
  CALI_TYPE_USR    = 0,
  CALI_TYPE_INT    = 1,
  CALI_TYPE_UINT   = 2,
  CALI_TYPE_STRING = 3,
  CALI_TYPE_ADDR   = 4,
  CALI_TYPE_DOUBLE = 5,
  CALI_TYPE_BOOL   = 6,
  CALI_TYPE_TYPE   = 7
} cali_attr_type;

const char* 
cali_type2string(cali_attr_type);

cali_attr_type 
cali_string2type(const char*);

typedef enum {
  CALI_ATTR_DEFAULT       =  0,
  CALI_ATTR_ASVALUE       =  1,
  CALI_ATTR_NOMERGE       =  2,
  CALI_ATTR_SCOPE_PROCESS = 12, /* make scope flags mutually exclusive when &'ed with SCOPE_MASK */ 
  CALI_ATTR_SCOPE_THREAD  = 20, 
  CALI_ATTR_SCOPE_TASK    = 24 
} cali_attr_properties;

#define CALI_ATTR_SCOPE_MASK 28

typedef enum {
  CALI_OP_SUM = 1,
  CALI_OP_MIN = 2,
  CALI_OP_MAX = 3
} cali_op;

typedef enum {
  CALI_SUCCESS = 0,
  CALI_EBUSY,
  CALI_ELOCKED,
  CALI_EINV
} cali_err;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CALI_CALI_TYPES_H
