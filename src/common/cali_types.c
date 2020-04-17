/* Copyright (c) 2019, Lawrence Livermore National Security, LLC.
 * See top-level LICENSE file for details.
 */

/**
 * @file cali_types.c
 * Context annotation library type conversion functions
 */

#include "caliper/common/cali_types.h"

#include <ctype.h>
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
  { "ptr",    CALI_TYPE_PTR    },
  { NULL,     CALI_TYPE_INV    }
};

const char*
cali_type2string(cali_attr_type type)
{
  return (type <= CALI_MAXTYPE ? typemap[type].str : "inv");
}

cali_attr_type
cali_string2type(const char* str)
{
  for (const struct typemap_t* t = typemap; t->str; ++t)
    if (strcmp(str, t->str) == 0)
      return t->type;

  return CALI_TYPE_INV;
}

static const struct propmap_t {
  const char* str; cali_attr_properties prop; int mask;
} propmap[] = {
  { "default",       CALI_ATTR_DEFAULT,       CALI_ATTR_DEFAULT     },
  { "asvalue",       CALI_ATTR_ASVALUE,       CALI_ATTR_ASVALUE     },
  { "nomerge",       CALI_ATTR_NOMERGE,       CALI_ATTR_NOMERGE     },
  { "process_scope", CALI_ATTR_SCOPE_PROCESS, CALI_ATTR_SCOPE_MASK  },
  { "thread_scope",  CALI_ATTR_SCOPE_THREAD,  CALI_ATTR_SCOPE_MASK  }, 
  { "task_scope",    CALI_ATTR_SCOPE_TASK,    CALI_ATTR_SCOPE_MASK  },
  { "skip_events",   CALI_ATTR_SKIP_EVENTS,   CALI_ATTR_SKIP_EVENTS },
  { "hidden",        CALI_ATTR_HIDDEN,        CALI_ATTR_HIDDEN      },
  { "nested",        CALI_ATTR_NESTED,        CALI_ATTR_NESTED      },
  { "global",        CALI_ATTR_GLOBAL,        CALI_ATTR_GLOBAL      },
  { "unaligned",     CALI_ATTR_UNALIGNED,     CALI_ATTR_UNALIGNED   },
  { 0, CALI_ATTR_DEFAULT, CALI_ATTR_DEFAULT }
};

int
cali_prop2string(int prop, char* buf, size_t len)
{
  int ret = 0;
  
  for (const struct propmap_t* p = propmap; p->str; ++p) {
      if (!((prop & p->mask) == (int) p->prop))
      continue;
    
    size_t slen = strlen(p->str);
    
    if ((slen + (ret>0?2:1)) > len)
      return -1;
    if (ret > 0)
      buf[ret++] = ':';
    
    strcpy(buf+ret, p->str);

    ret      += slen;
    buf[ret]  = '\0';
  }

  return ret;
}

int
cali_string2prop(const char* str)
{
  int prop = 0;
  
  for (const struct propmap_t* p = propmap; p->str; ++p) {
    const char* pos = strstr(str, p->str);
    size_t      len = strlen(p->str);

    if (pos && ((pos == str) || !isalnum(*(pos-1))) && !isalnum(pos[len]))
      prop |= p->prop;
  }

  return prop;
}
