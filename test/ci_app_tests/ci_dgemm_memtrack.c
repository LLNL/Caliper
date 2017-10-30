/*
 * dgemm test case
 * Used for datatracker and various service CI tests (sampler,callpath,symbollookup)
 */

#include <caliper/cali.h>
#include <caliper/cali_datatracker.h>

#include <stdlib.h>

static inline size_t
row_major(size_t x, size_t y, size_t width)
{
    return width*y + x;
}

double ci_dgemm_do_work(size_t M, size_t W, size_t N)
{
  CALI_MARK_FUNCTION_BEGIN;

  size_t i, j, k;

  CALI_MARK_BEGIN("alloc");

  double *matA = 
    (double*) cali_datatracker_allocate_dimensional("A", sizeof(double), (size_t[]) { M, W }, 2);
  double *matB = 
    (double*) cali_datatracker_allocate_dimensional("B", sizeof(double), (size_t[]) { W, N }, 2);
  double *matC = 
    (double*) cali_datatracker_allocate_dimensional("C", sizeof(double), (size_t[]) { M, N }, 2);

  CALI_MARK_END("alloc");

  CALI_MARK_BEGIN("setup");

  // Initialize A and B randomly
  for (i = 0; i < M; i++)
    for(k = 0; k < W; k++)
      matA[row_major(i,k,M)] = rand();

  for(k = 0; k < W; k++)
    for(j = 0; j < N; j++)
      matB[row_major(k,j,W)] = rand();

  CALI_MARK_END("setup");
  
  CALI_MARK_BEGIN("multiply");    

  // AB = C
  for (i = 0; i < M; i++)
    for(j = 0; j < N; j++)
      for(k = 0; k < W; k++)
        matC[row_major(i,j,M)] += matA[row_major(i,k,M)] * matB[row_major(k,j,W)];

  CALI_MARK_END("multiply");
  
  CALI_MARK_BEGIN("sum");
  // Print sum of elems in C
  double cSum = 0;
  for (i = 0; i < M; i++)
    for(j = 0; j < N; j++)
      cSum += matC[row_major(i,j,M)];
  CALI_MARK_END("sum");

  CALI_MARK_BEGIN("free");
  cali_datatracker_free(matA);
  cali_datatracker_free(matB);
  cali_datatracker_free(matC);
  CALI_MARK_END("free");

  CALI_MARK_FUNCTION_END;

  return cSum;
}

int main()
{
  CALI_MARK_FUNCTION_BEGIN;

  ci_dgemm_do_work(1024, 768, 512);

  CALI_MARK_FUNCTION_END;

  return 0;
}
