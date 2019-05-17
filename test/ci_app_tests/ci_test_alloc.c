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

  cali_variant_t  v_true = cali_make_variant_from_bool(true);

  cali_id_t ptr_in_attr  = 
    cali_create_attribute_with_metadata("ptr_in",  CALI_TYPE_ADDR, CALI_ATTR_ASVALUE,
                                        1, &cali_class_memoryaddress_attr_id, &v_true);
  cali_id_t ptr_out_attr = 
    cali_create_attribute_with_metadata("ptr_out", CALI_TYPE_ADDR, CALI_ATTR_ASVALUE,
                                        1, &cali_class_memoryaddress_attr_id, &v_true);

  int *A = 
    cali_datatracker_allocate_dimensional("test_alloc_A", sizeof(int), (const size_t[]) { 42 }, 1);

  cali_id_t  attrs[2] = { ptr_in_attr,  ptr_out_attr };
  int        scope    = CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD;

  cali_begin_byname("test_alloc.allocated.0");
  int* A_inside  = A;
  int* A_outside = A-1;
  
  cali_variant_t v_p_i  = 
    cali_make_variant(CALI_TYPE_ADDR, &A_inside, sizeof(int*));
  cali_variant_t v_p_o =
    cali_make_variant(CALI_TYPE_ADDR, &A_outside, sizeof(int*));

  cali_push_snapshot(scope, 2, attrs, (const cali_variant_t[]) { v_p_i, v_p_o });
  cali_end_byname("test_alloc.allocated.0");

  cali_begin_byname("test_alloc.allocated.1");
  A_inside  = A+41;
  /* int* A_outside = A+42; this should fail but doesn't: needs fix */
  A_outside = A+43;
  
  v_p_i = cali_make_variant(CALI_TYPE_ADDR, &A_inside, sizeof(int*));
  v_p_o = cali_make_variant(CALI_TYPE_ADDR, &A_outside, sizeof(int*));

  cali_push_snapshot(scope, 2, attrs, (const cali_variant_t[]) { v_p_i, v_p_o });
  cali_end_byname("test_alloc.allocated.1");

  cali_datatracker_free(A);

  cali_begin_byname("test_alloc.freed");
  cali_push_snapshot(scope, 2, attrs, (const cali_variant_t[]) { v_p_i, v_p_o });
  cali_end_byname("test_alloc.freed");

  CALI_MARK_FUNCTION_END;
}

int main()
{
  CALI_MARK_FUNCTION_BEGIN;

  ci_test_alloc();

  CALI_MARK_FUNCTION_END;

  return 0;
}
