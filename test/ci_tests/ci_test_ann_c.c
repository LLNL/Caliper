// --- Caliper continuous integration test app for C annotation interface

#include <cali.h>

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
}
