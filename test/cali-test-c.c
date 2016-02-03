/*
 * cali-test-c
 * Caliper C interface test
 */

#include <cali.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

void test_attr_by_name()
{
  cali_begin_attr_str("cali-test-c.experiment", "test_attr_by_name");

  cali_set_attr_dbl("implicit.double", 42.0);
  cali_end_attr("implicit.double");

  cali_set_attr_int("implicit.int", 42);
  cali_end_attr("implicit.int");
  
  cali_end_attr("cali-test-c.experiment");
}

void test_attr()
{
  cali_begin_attr_str("cali-test-c.experiment", "test_attr");

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

  cali_begin_str(s_attr, "first");

  const char* s = "second";
  cali_begin(s_attr, s, strlen(s));
  cali_end(s_attr);
  cali_end(s_attr);

  cali_end_attr("cali-test-c.experiment");
}

void test_mismatch()
{
  cali_begin_attr_str("cali-test-c.experiment", "mismatch");

  // --- int vs. string

  cali_begin_attr_str("cali-test-c.experiment", "int_vs_str");

  cali_id_t s_attr =
    cali_create_attribute("mismatch.str", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

  if (cali_set_int(s_attr, 141) == CALI_SUCCESS)
    puts("Undetected mismatch");
  
  cali_end_attr("cali-test-c.experiment");

  // --- int vs. string by name

  cali_begin_attr_str("cali-test-c.experiment", "int_vs_str_by_name");

  if (cali_set_attr_int("mismatch.str", 242) == CALI_SUCCESS)
    puts("Undetected mismatch");
  
  cali_end_attr("cali-test-c.experiment");

  // ---
  
  cali_end_attr("cali-test-c.experiment");
}

int main(int argc, char* argv[])
{
  test_attr_by_name();
  test_attr();
  test_mismatch();
  
  return 0;
}
