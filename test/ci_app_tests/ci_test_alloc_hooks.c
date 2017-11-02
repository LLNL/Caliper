/*
 * alloc test case
 * Used for alloc and callpath test cases
 */

#include <caliper/cali.h>
#include <caliper/cali_datatracker.h>

#include <stdlib.h>

extern cali_id_t cali_class_memoryaddress_attr_id;

void ci_test_alloc()
{
  CALI_MARK_FUNCTION_BEGIN;

  int         val_true    = 1;
  const void* val_ptrs[1] = { &val_true};
  size_t      val_size    = sizeof(int);

  cali_id_t ptr_in_attr  = 
    cali_create_attribute_with_metadata("ptr_in",  CALI_TYPE_ADDR, CALI_ATTR_ASVALUE,
                                        1, &cali_class_memoryaddress_attr_id, val_ptrs, &val_size);
  cali_id_t ptr_out_attr = 
    cali_create_attribute_with_metadata("ptr_out", CALI_TYPE_ADDR, CALI_ATTR_ASVALUE,
                                        1, &cali_class_memoryaddress_attr_id, val_ptrs, &val_size);

  int *A = (int*)malloc(sizeof(int)*42);

  cali_id_t  attrs[2] = { ptr_in_attr,  ptr_out_attr };
  size_t     sizes[2] = { sizeof(int*), sizeof(int*) };
  int        scope    = CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD;

  cali_begin_byname("test_alloc.allocated.0");
  int* A_inside  = A;
  int* A_outside = A-1;
  cali_push_snapshot(scope, 2, attrs, (const void*[]) { &A_inside, &A_outside }, sizes);
  cali_end_byname("test_alloc.allocated.0");

  cali_begin_byname("test_alloc.allocated.1");
  A_inside  = A+41;
  /* int* A_outside = A+42; this should fail but doesn't: needs fix */
  A_outside = A+43;
  cali_push_snapshot(scope, 2, attrs, (const void*[]) { &A_inside, &A_outside }, sizes);
  cali_end_byname("test_alloc.allocated.1");

  free(A);

  cali_begin_byname("test_alloc.freed");
  cali_push_snapshot(scope, 2, attrs, (const void*[]) { &A_inside, &A_outside}, sizes);
  cali_end_byname("test_alloc.freed");

  CALI_MARK_FUNCTION_END;
}

int main()
{
  CALI_MARK_FUNCTION_BEGIN;

  ci_test_alloc();

  CALI_MARK_FUNCTION_END;
}
