/*
 * cali-basic-c
 * Caliper C interface test
 */

#include <cali.h>

#include <stdlib.h>


void foo(int count)
{
  /* foo pre-processing */
  
  /* Create attribute key for iteration information */

  cali_id_t iter_attr =
    cali_create_attribute("cali-demo.foo.iteration", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

  /* Mark foo loop and set loopcount information */

  cali_begin_byname("cali-demo.fooloop");
  cali_set_int_byname("cali-demo.foo.loopcount", count);
  
  for (int i = 0; i < count; ++i) {
    cali_set_int(iter_attr, i);  /* Mark each iteration */

    /* do work */
  }
  cali_end(iter_attr);        /* clear 'cali-demo.foo.iteration'  */

  cali_end_byname("cali-demo.fooloop");

  /* foo post-processing */
}

int main(int argc, char* argv[])
{
  cali_begin_byname("cali-demo.main");

  /* --- Mark initialization phase --- */ 

  cali_begin_byname("cali-demo.initialization");
  int count = argc > 1 ? atoi(argv[1]) : 4;
  cali_end_byname("cali-demo.initialization");


  /* --- Mark foo kernel --- */

  cali_begin_byname("cali-demo.foo");
  foo(count);
  cali_end_byname("cali-demo.foo");


  /* --- Finalization --- */

  cali_end_byname("cali-basic-c.main");
}
