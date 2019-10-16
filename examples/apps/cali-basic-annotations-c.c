/* Copyright (c) 2019, Lawrence Livermore National Security, LLC.
 * See top-level LICENSE file for details.
 */

/*
 * cali-basic-c
 * Caliper C interface example
 *
 * Demonstrate the use of Caliper's annotation macros in a C program.
 */

#include <caliper/cali.h>

#include <stdlib.h>


void foo(int count)
{
  /* Mark begin of a function. Sets "function=foo" in Caliper. */
  CALI_MARK_FUNCTION_BEGIN;

  /* Export "cali-demo.foo.loopcount=<count>" via the low-level C annotation API. */
  cali_set_int_byname("cali-demo.foo.loopcount", count);

  double *d = (double*) malloc(count*sizeof(double));

  if (!d) {
    /* All function exits must be marked! */
    CALI_MARK_FUNCTION_END;
    return;
  }

  /* Mark a loop. Sets "loop=cali-demo.fooloop" in Caliper. */ 
  CALI_MARK_LOOP_BEGIN(fooloop, "cali-demo.fooloop");
  
  for (int i = 0; i < count; ++i) {
    /* Mark a loop iteration. Sets "iteration#cali-demo.fooloop=<i>" in Caliper. */
    CALI_MARK_ITERATION_BEGIN(fooloop, i);

    /* do work */
    d[i] = i;

    CALI_MARK_ITERATION_END(fooloop);
  }

  CALI_MARK_LOOP_END(fooloop);

  free(d);

  CALI_MARK_FUNCTION_END;
}

int main(int argc, char* argv[])
{
  CALI_MARK_FUNCTION_BEGIN;

  /* Marking a single C statement. Sets "statement=cali-demo.init" in Caliper. */ 
  CALI_WRAP_STATEMENT("cali-demo.init", int count = argc > 1 ? atoi(argv[1]) : 4);

  foo(count);

  CALI_MARK_FUNCTION_END;
}
