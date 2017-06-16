/*
 * cali-basic-c
 * Caliper C interface test
 */

#include <caliper/cali.h>

#include <stdlib.h>


void foo(int count)
{
  CALI_MARK_FUNCTION_BEGIN;

  cali_set_int_byname("cali-demo.foo.loopcount", count);

  /* Mark foo loop and set loopcount information */

  CALI_MARK_LOOP_BEGIN(fooloop, "cali-demo.fooloop");
  
  for (int i = 0; i < count; ++i) {
    CALI_MARK_ITERATION_BEGIN(fooloop, i);

    /* do work */

    CALI_MARK_ITERATION_END(fooloop);
  }

  CALI_MARK_LOOP_END(fooloop);
  CALI_MARK_FUNCTION_END;
}

int main(int argc, char* argv[])
{
  CALI_MARK_FUNCTION_BEGIN;

  /* --- Mark initialization phase --- */ 

  CALI_WRAP_STATEMENT("cali-demo.init", int count = argc > 1 ? atoi(argv[1]) : 4);

  /* --- Mark foo kernel --- */

  foo(count);

  /* --- Finalization --- */

  CALI_MARK_FUNCTION_END;
}
