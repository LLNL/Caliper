#ifndef IO_CALLBACKS
  #define IO_CALLBACKS
 
  #include <curious/curious.h>

  #ifdef __cplusplus
  extern "C" {
  #endif

  /*********
   * Types *
   *********/

  typedef struct curious_callback_data {
    //a function to be called either before or after an IO function
    curious_callback_f callback;
    //arbitrary usr-defined information which can be passed to the callback function
    //(note: the user is responsible for freeing this, if necessary)
    void *usr_args;
    //indicates which CURIOUS instance registered this callback
    curious_t curious_inst;
  } curious_callback_data_t;

  /**********************
   * Internal Functions *
   **********************/

  void curious_call_callbacks(curious_callback_type_t type, void *io_args);
  void curious_destroy_curious_callback_data(curious_callback_data_t *self);
  void curious_init_callback_registry(void);
  void curious_finalize_callback_registry(void);

  // Prefix is just for consistency with curious_register_callback, not an indication
  // that this function is a part of the external API
  void curious_deregister_callbacks(curious_t curious_inst);

  /*****************
   * API Functions *
   *****************/

  // See curious.h under include/curious for API functions

  #ifdef __cplusplus
  } // extern "C"
  #endif
#endif
