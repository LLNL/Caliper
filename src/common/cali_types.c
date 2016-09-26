/**
 * @file cali_types.c
 * Context annotation library type conversion functions
 */

#include "cali_types.h"

#include <string.h>

static const struct typemap_t {
  const char* str; cali_attr_type type;
} typemap[] = { 
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
  return (type >= 0 && type <= CALI_MAXTYPE ? typemap[type].str : "invalid");
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
  const char* str; cali_attr_properties prop;
} propmap[] = {
  { "default",       CALI_ATTR_DEFAULT,      },
  { "asvalue",       CALI_ATTR_ASVALUE       },
  { "nomerge",       CALI_ATTR_NOMERGE       },
  { "process_scope", CALI_ATTR_SCOPE_PROCESS },
  { "thread_scope",  CALI_ATTR_SCOPE_THREAD  }, 
  { "task_scope",    CALI_ATTR_SCOPE_TASK    },
  { "skip_events",   CALI_ATTR_SKIP_EVENTS   },
  { "hidden",        CALI_ATTR_HIDDEN        },
  { 0, CALI_ATTR_DEFAULT }
};

int
cali_prop2string(int prop, char* buf, size_t len)
{
  int ret = 0;
  
  for (const struct propmap_t* p = propmap; p->str; ++p) {
    if (!(p->prop & prop))
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

    if (pos && !(pos > str && *(pos-1) != ':') && (pos[len] == '\0' || pos[len] == ':'))
      prop |= p->prop;
  }

  return prop;
}
