/**
 * @file cali_types.c
 * Context annotation library type conversion functions
 */

#include "cali_types.h"

#include <string.h>

static const struct typemap_t {
  const char* str; cali_attr_type type;
} typemap[] = { 
  { "inv",    CALI_TYPE_INV    },
  { "usr",    CALI_TYPE_USR    },
  { "int",    CALI_TYPE_INT    },
  { "uint",   CALI_TYPE_UINT   },
  { "string", CALI_TYPE_STRING },
  { "addr",   CALI_TYPE_ADDR   },
  { "double", CALI_TYPE_DOUBLE },
  { "bool",   CALI_TYPE_BOOL   },
  { "type",   CALI_TYPE_TYPE   },
  { NULL,     CALI_TYPE_INV    }
};

const char*
cali_type2string(cali_attr_type type)
{
  return (type >= 0 && type <= CALI_MAXTYPE ? typemap[type].str : "inv");
}

cali_attr_type
cali_string2type(const char* str)
{
  for (const struct typemap_t* t = typemap; t->str; ++t)
    if (strcmp(str, t->str) == 0)
      return t->type;

  return CALI_TYPE_INV;
}
