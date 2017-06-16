// --- Caliper continuous integration test app for C annotation interface

#include "caliper/cali.h"

int main()
{
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
  int val  = 47;
  size_t meta_sizes = sizeof(int);
  const void* meta_val[1] = { &val };

  cali_id_t test_attr =
      cali_create_attribute_with_metadata("test-attr-with-metadata", CALI_TYPE_STRING, CALI_ATTR_DEFAULT,
                                          1, &meta_attr, meta_val, &meta_sizes);

  cali_set_string(test_attr, "abracadabra");
  
  cali_end_byname("ci_test_c_ann.meta-attr");
  
  cali_begin_byname("ci_test_c_ann.setbyname");

  cali_set_int_byname("attr.int", 20);
  cali_set_double_byname("attr.dbl", 1.25);
  cali_set_string_byname("attr.str", "fidibus");

  cali_end_byname("ci_test_c_ann.setbyname");
}
