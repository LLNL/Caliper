#include <Annotation.h>
#include <Caliper.h>

#include <cstdlib>
#include <iostream>

#ifdef TEST_USE_OMP
#include <omp.h>
#endif

#define ROW_MAJOR(x,y,width) y*width+x

void init_matrices(int N, double **a, double **b, double **c)
{
    cali::Annotation::AutoScope si( cali::Annotation("matrix").begin("initialize") );

    *a = new double[N*N];
    *b = new double[N*N];
    *c = new double[N*N];

    for(int i=0; i<N; ++i)
    {
        for(int j=0; j<N; ++j)
        {
            (*a)[ROW_MAJOR(i,j,N)] = (double)rand();
            (*b)[ROW_MAJOR(i,j,N)] = (double)rand();
            (*c)[ROW_MAJOR(i,j,N)] = 0;
        }
    }
}

void matmul(int N, double *a, double *b, double *c)
{
    cali::Annotation::AutoScope so( cali::Annotation("matrix").begin("multiply") );
#ifdef TEST_USE_OMP
#pragma omp parallel 
    {
        cali::Annotation("omp.thread").set(omp_get_thread_num());

        cali::Annotation::AutoScope 
            st( cali::Annotation("matrix").begin("thread_multiply") );
#pragma omp for    
#endif
        for(int i=0; i<N; ++i)
        {
            for(int j=0; j<N; ++j)
            {
                for(int k=0; k<N; ++k)
                {
                    c[ROW_MAJOR(i,j,N)] += a[ROW_MAJOR(i,k,N)]*b[ROW_MAJOR(k,j,N)];
                }
            }
        }
#ifdef TEST_USE_OMP
    }
#endif
}

int main(int argc, char **argv)
{
    int N = (argc == 2) ? atoi(argv[1]) : 1024;
    double *a,*b,*c;

    cali::Annotation phase_annotation("phase");
    cali::Annotation::AutoScope sp( phase_annotation.begin("main") );

    init_matrices(N,&a,&b,&c);
    matmul(N,a,b,c);
}
