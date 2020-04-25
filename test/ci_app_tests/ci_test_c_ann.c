// --- Caliper continuous integration test app for C annotation interface

#include "../../src/interface/c_fortran/wrapConfigManager.h"

#include "caliper/cali.h"

#include <stdio.h>

int main(int argc, char* argv[])
{
  cali_config_preset("CALI_CHANNEL_FLUSH_ON_EXIT", "false");

  cali_ConfigManager mgr;
  cali_ConfigManager_new(&mgr);

  if (argc > 1)
    cali_ConfigManager_add(&mgr, argv[1]);
  if (cali_ConfigManager_error(&mgr)) {
    cali_SHROUD_array errmsg;
    cali_ConfigManager_error_msg_bufferify(&mgr, &errmsg);
    fprintf(stderr, "Caliper config error: %s\n", errmsg.addr.ccharp);
    cali_SHROUD_memory_destructor(&errmsg.cxx);
    cali_ConfigManager_delete(&mgr);
    return -1;
  }

  cali_ConfigManager_start(&mgr);

  cali_set_global_double_byname("global.double", 42.42);
  cali_set_global_int_byname("global.int", 1337);
  cali_set_global_string_byname("global.string", "my global string");
  cali_set_global_uint_byname("global.uint", 42);

  cali_id_t iter_attr =
    cali_create_attribute("iteration", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

  cali_begin_string_byname("phase", "loop");

  for (int i = 0; i < 4; ++i) {
    cali_begin_int(iter_attr, i);
    cali_end(iter_attr);
  }

  cali_end_byname("phase");

  cali_begin_byname("ci_test_c_ann.meta-attr");

  cali_id_t meta_attr =
    cali_create_attribute("meta-attr", CALI_TYPE_INT, CALI_ATTR_DEFAULT);
  cali_variant_t meta_val =
    cali_make_variant_from_int(47);

  cali_id_t test_attr =
      cali_create_attribute_with_metadata("test-attr-with-metadata", CALI_TYPE_STRING, CALI_ATTR_NOMERGE,
                                          1, &meta_attr, &meta_val);

  cali_set_string(test_attr, "abracadabra");

  cali_end_byname("ci_test_c_ann.meta-attr");

  cali_begin_byname("ci_test_c_ann.setbyname");

  cali_set_int_byname("attr.int", 20);
  cali_set_double_byname("attr.dbl", 1.25);
  cali_set_string_byname("attr.str", "fidibus");

  cali_end_byname("ci_test_c_ann.setbyname");

  cali_ConfigManager_flush(&mgr);
  cali_ConfigManager_delete(&mgr);

  cali_flush(CALI_FLUSH_CLEAR_BUFFERS);

  return 0;
}
