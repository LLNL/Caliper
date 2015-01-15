#ifndef CALI_CALI_DEFINITIONS_H
#define CALI_CALI_DEFINITIONS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  CALI_SCOPE_PROCESS = 0,
  CALI_SCOPE_THREAD  = 1,
  CALI_SCOPE_TASK    = 2 
} cali_context_scope_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
