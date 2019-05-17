/*
 * alloc test case
 * Used for alloc and callpath test cases
 */

#include <caliper/cali.h>
#include <caliper/cali_datatracker.h>

#include <stdlib.h>

extern cali_id_t cali_class_memoryaddress_attr_id;

cali_id_t  attrs[2];
int        scope = CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD;

void test_allocation(void *ptr, size_t size)
{
  cali_begin_byname("test_alloc.allocated.0");
  
  int* ptr_inside  = ptr;
  int* ptr_outside = ptr-1;
  cali_variant_t v_p_i  = 
    cali_make_variant(CALI_TYPE_ADDR, &ptr_inside, sizeof(void*));
  cali_variant_t v_p_o =
    cali_make_variant(CALI_TYPE_ADDR, &ptr_outside, sizeof(void*));
  
  cali_push_snapshot(scope, 2, attrs, (const cali_variant_t[]) { v_p_i, v_p_o });
  cali_end_byname("test_alloc.allocated.0");

  cali_begin_byname("test_alloc.allocated.1");
  ptr_inside  = ptr+size-1;
  ptr_outside = ptr+size;
  
  v_p_i = cali_make_variant(CALI_TYPE_ADDR, &ptr_inside, sizeof(void*));
  v_p_o = cali_make_variant(CALI_TYPE_ADDR, &ptr_outside, sizeof(void*));

  cali_push_snapshot(scope, 2, attrs, (const cali_variant_t[]) { v_p_i, v_p_o });
  cali_end_byname("test_alloc.allocated.1");
}

void test_free(void *ptr)
{
  cali_begin_byname("test_alloc.freed");
  free(ptr);

  int* ptr_inside  = ptr;
  int* ptr_outside = ptr-1;
  cali_variant_t v_p_i  = 
    cali_make_variant(CALI_TYPE_ADDR, &ptr_inside, sizeof(void*));
  cali_variant_t v_p_o =
    cali_make_variant(CALI_TYPE_ADDR, &ptr_outside, sizeof(void*));

  cali_push_snapshot(scope, 2, attrs, (const cali_variant_t[]) { v_p_i, v_p_o });
  cali_end_byname("test_alloc.freed");
}

void ci_test_alloc()
{
  CALI_MARK_FUNCTION_BEGIN;

  cali_variant_t v_true = cali_make_variant_from_bool(true);

  cali_id_t ptr_in_attr  = 
    cali_create_attribute_with_metadata("ptr_in",  CALI_TYPE_ADDR, CALI_ATTR_ASVALUE,
                                        1, &cali_class_memoryaddress_attr_id, &v_true);
  cali_id_t ptr_out_attr = 
    cali_create_attribute_with_metadata("ptr_out", CALI_TYPE_ADDR, CALI_ATTR_ASVALUE,
                                        1, &cali_class_memoryaddress_attr_id, &v_true);

  attrs[0] = ptr_in_attr;
  attrs[1] = ptr_out_attr;

  int *A = (int*)malloc(sizeof(int)*42);
  int *C = (int*)calloc(42, sizeof(int));
  int *R = (int*)malloc(sizeof(int)*100);
  R = (int*)realloc(R, sizeof(int)*42);

  cali_begin_byname("test_alloc.malloc_hook");
  test_allocation(A, sizeof(int)*42);
  test_free(A);
  cali_end_byname("test_alloc.malloc_hook");

  cali_begin_byname("test_alloc.calloc_hook");
  test_allocation(C, sizeof(int)*42);
  test_free(C);
  cali_end_byname("test_alloc.calloc_hook");

  cali_begin_byname("test_alloc.realloc_hook");
  test_allocation(R, sizeof(int)*42);
  test_free(R);
  cali_end_byname("test_alloc.realloc_hook");

  CALI_MARK_FUNCTION_END;
}

int main()
{
  CALI_MARK_FUNCTION_BEGIN;

  ci_test_alloc();

  CALI_MARK_FUNCTION_END;

  return 0;
}
