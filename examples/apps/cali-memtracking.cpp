// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/tools-util/Args.h"

#include "caliper/cali_datatracker.h"

#include <caliper/Annotation.h>
#include <caliper/Caliper.h>

#include <numeric>

size_t row_major(size_t x, size_t y, size_t width)
{
    return width*y + x;
}

void do_work(size_t M, size_t W, size_t N)
{
    size_t i, j, k;

    cali::Annotation alloc_phase(cali::Annotation("phase").begin("allocate"));

    const size_t dimA[] = { M, W };
    const size_t dimB[] = { W, N };
    const size_t dimC[] = { M, N };

    double *matA = 
        (double*) cali_datatracker_allocate_dimensional("A", sizeof(double), dimA, 2);
    double *matB =
        (double*) cali_datatracker_allocate_dimensional("B", sizeof(double), dimB, 2);
    double *matC =
        (double*) cali_datatracker_allocate_dimensional("C", sizeof(double), dimC, 2);

    alloc_phase.end();
    cali::Annotation init_phase(cali::Annotation("phase").begin("initialize_values"));

    // Initialize A and B randomly
    for (i = 0; i < M; i++)
        for(k = 0; k < W; k++)
            matA[row_major(i,k,M)] = rand();

    for(k = 0; k < W; k++)
        for(j = 0; j < N; j++)
            matB[row_major(k,j,W)] = rand();

    init_phase.end();
    cali::Annotation mul_phase(cali::Annotation("phase").begin("multiply"));

    // AB = C
    for (i = 0; i < M; i++)
        for(j = 0; j < N; j++)
            for(k = 0; k < W; k++)
                matC[row_major(i,j,M)] += matA[row_major(i,k,M)] * matB[row_major(k,j,W)];

    mul_phase.end();
    cali::Annotation sum_phase(cali::Annotation("phase").begin("sum"));

    // Print sum of elems in C
    double cSum = 0;
    for (i = 0; i < M; i++)
        for(j = 0; j < N; j++)
            cSum += matC[row_major(i,j,M)];

    std::cout << "cSum = " << cSum << std::endl;

    sum_phase.end();
    cali::Annotation free_phase(cali::Annotation("phase").begin("free"));

    cali_datatracker_free(matA);
    cali_datatracker_free(matB);
    cali_datatracker_free(matC);

    free_phase.end();
}

int main(int argc, const char* argv[])
{
    // parse command line arguments

    const util::Args::Table option_table[] = {
            { "m_size",      "m_size",      'm', true,
              "Width of input matrix A",
              "elements" },
            { "w_size",      "w_size",      'w', true,
              "Height of input matrix A and width of input matrix B",
              "elements" },
            { "n_size",      "n_size",      'n', true,
              "Height of input matrix B",
              "elements" },
            { "iterations",      "iterations",      'i', true,
              "Number of iterations",
              "iterations" },

            util::Args::Table::Terminator
    };

    util::Args args(option_table);

    int lastarg = args.parse(argc, argv);

    if (lastarg < argc) {
        std::cerr << "cali-throughput-thread: unknown option: " << argv[lastarg] << '\n'
                  << "  Available options: ";

        args.print_available_options(std::cerr);

        return -1;
    }

    size_t m_size = std::stoul(args.get("m_size", "512"));
    size_t w_size = std::stoul(args.get("w_size", "512"));
    size_t n_size = std::stoul(args.get("n_size", "512"));
    size_t num_iterations = std::stoul(args.get("iterations", "4"));

    cali::Annotation phase_annotation("phase", CALI_ATTR_SCOPE_PROCESS);

    phase_annotation.begin("benchmark");

    cali::Loop loop("loop");
    for(size_t i=0; i<num_iterations; i++) {
        cali::Loop::Iteration iteration(loop.iteration((int)i));
        do_work(m_size, w_size, n_size);
    }

    phase_annotation.end();
}
