#ifndef IO_WRAPPERS
  #define IO_WRAPPERS

  #include <sys/types.h>
  #include <sys/stat.h>
  #include <stdio.h>
  #include <gotcha/gotcha.h>
  #include <curious/curious.h>

  #ifdef __cplusplus
  extern "C" {
  #endif

  /*********
   * Types *
   *********/
  
  // This just keeps all the data about the original function in one place
  typedef struct io_function_data {
    curious_api_t api;
    gotcha_wrappee_handle_t handle;
  } io_function_data_t;

  void curious_apply_wrappers(curious_apis_t apis);
  void curious_disable_wrappers(void);

  // See io_arhs.h under include/curious for structs to serve as io_args 
  // to callbacks for corresponding functions
  
  #ifdef __cplusplus
  } // extern "C"
  #endif
#endif
