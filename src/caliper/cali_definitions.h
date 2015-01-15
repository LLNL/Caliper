#ifndef CALI_CALI_DEFINITIONS_H
#define CALI_CALI_DEFINITIONS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  CALI_SCOPE_PROCESS,
  CALI_SCOPE_THREAD,
  CALI_SCOPE_TASK
} cali_context_scope_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
