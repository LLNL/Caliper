/*
 * cali-test-c
 * Caliper C interface test
 */

#include <caliper/cali.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

void test_attr_by_name()
{
  cali_begin_string_byname("cali-test-c.experiment", "test_attr_by_name");

  cali_set_double_byname("implicit.double", 42.0);
  cali_end_byname("implicit.double");

  cali_set_int_byname("implicit.int", 42);
  cali_end_byname("implicit.int");
  
  cali_end_byname("cali-test-c.experiment");
}

void test_attr()
{
  cali_begin_string_byname("cali-test-c.experiment", "test_attr");

  // --- int
  
  cali_id_t i_attr =
    cali_create_attribute("explicit.int", CALI_TYPE_INT, CALI_ATTR_DEFAULT);

  cali_set_int(i_attr, 141);

  int64_t i = 242;

  cali_set(i_attr, &i, sizeof(i));
  cali_end(i_attr);

  // --- string
  
  cali_id_t s_attr =
    cali_create_attribute("explicit.str", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

  cali_begin_string(s_attr, "first");

  const char* s = "second";
  cali_set(s_attr, s, strlen(s));
  cali_end(s_attr);
  cali_end(s_attr);

  cali_end_byname("cali-test-c.experiment");
}

void test_mismatch()
{
  cali_begin_string_byname("cali-test-c.experiment", "mismatch");

  // --- int vs. string

  cali_begin_string_byname("cali-test-c.experiment", "int_vs_str");

  cali_id_t s_attr =
    cali_create_attribute("mismatch.str", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

  if (cali_set_int(s_attr, 141) == CALI_SUCCESS)
    puts("Undetected mismatch");
  
  cali_end_byname("cali-test-c.experiment");

  // --- int vs. string by name

  cali_begin_string_byname("cali-test-c.experiment", "int_vs_str_by_name");

  if (cali_set_int_byname("mismatch.str", 242) == CALI_SUCCESS)
    puts("Undetected mismatch");
  
  cali_end_byname("cali-test-c.experiment");

  // ---
  
  cali_end_byname("cali-test-c.experiment");
}

void test_metadata()
{
  cali_begin_string_byname("cali-test-c.experiment", "metadata");

  cali_id_t meta_str_attr =
    cali_create_attribute("meta-string-attr", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
  cali_id_t meta_int_attr =
    cali_create_attribute("meta-int-attr",    CALI_TYPE_INT,    CALI_ATTR_DEFAULT);

  int         meta_i = 77;
  const char* meta_s = "metastring";
    
  cali_id_t   meta_attr[2] = { meta_str_attr,  meta_int_attr };
  const void* meta_vals[2] = { meta_s,         &meta_i       };
  size_t      meta_size[2] = { strlen(meta_s), sizeof(int)   };

  cali_id_t attr =
    cali_create_attribute_with_metadata("metatest.attr", CALI_TYPE_STRING, CALI_ATTR_DEFAULT,
                                        2, meta_attr, meta_vals, meta_size);

  cali_set_string(attr, "testing");
  cali_end(attr);
  
  cali_end_byname("cali-test-c.experiment");
}

void test_snapshot()
{
  cali_begin_string_byname("cali-test-c.experiment", "snapshot");

  /* Test w/o event trigger info */ 
  cali_push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD,
                     0, NULL, NULL, NULL);
  
  cali_id_t event_str_attr =
    cali_create_attribute("myevent-string", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
  cali_id_t event_val_attr =
    cali_create_attribute("myevent-value",  CALI_TYPE_INT,    CALI_ATTR_ASVALUE);

  int         event_val = 42;
  const char* event_str = "myevent";
  
  cali_id_t   event_attr[2] = { event_str_attr,    event_val_attr };
  const void* event_data[2] = { event_str,         &event_val     };
  size_t      event_size[2] = { strlen(event_str), sizeof(int)    };

  cali_push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD,
                     2, event_attr, event_data, event_size);

  cali_end_byname("cali-test-c.experiment");    
}

int main(int argc, char* argv[])
{
  test_attr_by_name();
  test_attr();
  test_mismatch();
  test_metadata();
  test_snapshot();
  
  return 0;
}
