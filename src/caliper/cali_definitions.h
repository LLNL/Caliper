#ifndef CALI_CALI_DEFINITIONS_H
#define CALI_CALI_DEFINITIONS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  CALI_SCOPE_PROCESS = 1,
  CALI_SCOPE_THREAD  = 2,
  CALI_SCOPE_TASK    = 4 
} cali_context_scope_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
