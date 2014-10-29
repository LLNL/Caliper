/**
 * @file cali_types.c
 * Context annotation library type conversion functions
 */

#include "cali_types.h"

#include <string.h>

static const struct typemap_t {
  const char* str; ctx_attr_type type;
} typemap[] = { 
  { "usr",    CTX_TYPE_USR    },
  { "int",    CTX_TYPE_INT    },
  { "uint",   CTX_TYPE_UINT   },
  { "string", CTX_TYPE_STRING },
  { "addr",   CTX_TYPE_ADDR   },
  { "double", CTX_TYPE_DOUBLE },
  { "bool",   CTX_TYPE_BOOL   },
  { "type",   CTX_TYPE_TYPE   },
  { NULL,     CTX_TYPE_INV    }
};

static const int typemax = CTX_TYPE_TYPE;

const char*
cali_type2string(ctx_attr_type type)
{
  return (type >= 0 && type <= CTX_TYPE_TYPE ? typemap[type].str : "invalid");
}

ctx_attr_type
cali_string2type(const char* str)
{
  for (const struct typemap_t* t = typemap; t->str; ++t)
    if (strcmp(str, t->str) == 0)
      return t->type;

  return CTX_TYPE_INV;
}
