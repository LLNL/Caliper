#include <curious/curious.h>
#include <stdio.h>

#include "callbacks.h"
#include "wrappers.h"
#include "file_registry.h"
#include "mount_tree.h"

static curious_t next_inst = 0;
static int active_insts = 0;
static curious_apis_t wrapped_apis = 0;

int curious_init(curious_apis_t apis) {
  if (0 == active_insts) {
    //First-time setup
    curious_init_file_registry();
    curious_init_callback_registry();
  }
  
  ++active_insts;

  //If there are any APIs which were requested
  //that aren't already wrapped, wrap them
  curious_apis_t missing_apis = apis & ~wrapped_apis;
  if (missing_apis) {
    curious_apply_wrappers(missing_apis);
    wrapped_apis |= missing_apis;
  }

  return ++next_inst;
}

void curious_finalize(curious_t curious_inst) {
  //Turn everything off if we don't have any active instances...
  if(0 == --active_insts) {
    curious_finalize_callback_registry();
    curious_disable_wrappers();
    curious_finalize_file_registry();

  // ...and just remove the callbacks from this instance of CURIOUS otherwise
  } else {
    curious_deregister_callbacks(curious_inst);
  }
}
