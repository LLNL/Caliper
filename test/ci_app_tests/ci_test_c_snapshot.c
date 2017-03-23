// Caliper continuous integration test app for C snapshot interface

#include <cali.h>

#include <string.h>

int main()
{
  cali_begin_string_byname("ci_test_c", "snapshot");

  /* Test w/o event trigger info */ 
  cali_push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD,
                     0, NULL, NULL, NULL);
  
  cali_id_t event_str_attr =
    cali_create_attribute("string_arg", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
  cali_id_t event_val_attr =
    cali_create_attribute("int_arg",    CALI_TYPE_INT,    CALI_ATTR_ASVALUE);

  int         event_val = 42;
  const char* event_str = "teststring";
  
  cali_id_t   event_attr[2] = { event_str_attr,    event_val_attr };
  const void* event_data[2] = { event_str,         &event_val     };
  size_t      event_size[2] = { strlen(event_str), sizeof(int)    };

  cali_push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD,
                     2, event_attr, event_data, event_size);

  cali_end_byname("ci_test_c");
}
