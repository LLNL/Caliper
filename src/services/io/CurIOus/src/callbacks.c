#include <stdlib.h>

#include "wrappers.h"
#include "callbacks.h"
#include "dynamic_array.h"

static curious_dynamic_array_t callback_registry[CURIOUS_CALLBACK_TYPE_COUNT];

void curious_init_callback_registry(void) {
  for (int i = 0; i < CURIOUS_CALLBACK_TYPE_COUNT; ++i) {
    create_curious_dynamic_array(callback_registry + i, 0, sizeof(curious_callback_data_t));
  }
}

void curious_finalize_callback_registry(void) {
  for (int i = 0; i < CURIOUS_CALLBACK_TYPE_COUNT; ++i) {
    destroy_curious_dynamic_array(callback_registry + i, NULL);
  }
}

int curious_register_callback(
  curious_t curious_inst,
  curious_callback_type_t type,
  curious_callback_f callback,
  void *usr_args
) {
  if (0 <= type && type < CURIOUS_CALLBACK_TYPE_COUNT) {
    //we don't need to allocate space to hold the data, since it's just going to be memcpy-ed into an array
    curious_callback_data_t data;

    //initialise feilds
    data.callback = callback;
    data.usr_args = usr_args;
    data.curious_inst = curious_inst;

    //add record to the part of the registry corresponding to its type
    append_to_curious_dynamic_array(callback_registry + type, (void *) &data);

    return 0;
  } else {
    return CURIOUS_ERR_INVALID_CALLBACK_TYPE;
  }
}

void curious_deregister_callbacks(curious_t curious_inst) {
  for (int i = 0; i < CURIOUS_CALLBACK_TYPE_COUNT; ++i) {
    curious_dynamic_array_t *callbacks = callback_registry + i;
    int block_size = 0;
    for (int j = 0; j <= callbacks->last_el; ++j) {
      curious_callback_data_t *data = (curious_callback_data_t *) (get_from_curious_dynamic_array(callbacks, j));
     
      // We want to deal with contiguous blocks of functions from this CURIOUS instance
      // all at once, for effeciency reasons, so when we find a callback that comes
      // from that instance, we just note that we found it, for now...
      if (data->curious_inst == curious_inst) {
        ++block_size;
      // ...when we reach the end of such a block, that's when we actually remove
      // all of the call backs...
      } else if (block_size) {
        remove_from_curious_dynamic_array(callbacks, j - block_size, block_size, NULL);
        // ...and then start looking for a new block
        block_size = 0;
      }
    }
    
    // We might have a block go to the end of the array, but we still need to remove those
    if (block_size) {
      remove_from_curious_dynamic_array(callbacks, callbacks->last_el + 1 - block_size, block_size, NULL);
    }
  }
}

void curious_call_callbacks(curious_callback_type_t type, void *io_args) {
  //lookup the list of registred functions of the specified type...
  curious_dynamic_array_t callbacks = callback_registry[type];

  //...and run every one
  for (int i = 0; i <= callbacks.last_el; ++i) {
    curious_callback_data_t *data = (curious_callback_data_t *) (get_from_curious_dynamic_array(&callbacks, i));
    
    //usr_data was registered with the function earlier,
    //io_args was just passed to the IO function in the main code
    data->callback(type, data->usr_args, io_args);
  }
}
