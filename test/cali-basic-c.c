/*
 * cali-basic-c
 * Caliper C interface test
 */

#include <cali.h>

#include <stdlib.h>

int main(int argc, char* argv[])
{
  cali_begin_byname("cali-basic-c.main");

  int count = argc > 1 ? atoi(argv[1]) : 4;

  cali_id_t attr_iter =
    cali_create_attribute("iteration", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

  cali_begin_byname("cali-basic-c.loop");

  for (int i = 0; i < count; ++i) {
    cali_set_int(attr_iter, i);
  }

  cali_end(attr_iter);      /* clear 'iteration'     */

  cali_end_byname("cali-basic-c.loop");
  cali_end_byname("cali-basic-c.main");
}
