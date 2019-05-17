// --- Caliper continuous integration test app for C annotation interface

#include "caliper/cali.h"

int main()
{
  cali_config_preset("CALI_CALIPER_FLUSH_ON_EXIT", "false");

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
      cali_create_attribute_with_metadata("test-attr-with-metadata", CALI_TYPE_STRING, CALI_ATTR_DEFAULT,
                                          1, &meta_attr, &meta_val);

  cali_set_string(test_attr, "abracadabra");
  
  cali_end_byname("ci_test_c_ann.meta-attr");
  
  cali_begin_byname("ci_test_c_ann.setbyname");

  cali_set_int_byname("attr.int", 20);
  cali_set_double_byname("attr.dbl", 1.25);
  cali_set_string_byname("attr.str", "fidibus");

  cali_end_byname("ci_test_c_ann.setbyname");

  cali_flush(CALI_FLUSH_CLEAR_BUFFERS);

  return 0;
}
