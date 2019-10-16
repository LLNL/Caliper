// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/tools-util/Args.h"

#include <caliper/cali.h>
#include <caliper/cali_datatracker.h>

#include <numeric>

size_t row_major(size_t x, size_t y, size_t width)
{
    return width*y + x;
}

void do_work(size_t M, size_t W, size_t N)
{
    size_t i, j, k;

    CALI_MARK_BEGIN("allocate");

    double *matA = (double*)malloc(sizeof(double)*M*W);
    double *matB = (double*)malloc(sizeof(double)*W*N);
    double *matC = (double*)malloc(sizeof(double)*M*N);

    size_t num_dimensions = 2;
    size_t A_dims[] = {M,W};
    size_t B_dims[] = {W,N};
    size_t C_dims[] = {M,N};

    CALI_DATATRACKER_TRACK_DIMENSIONAL(matA, sizeof(double), A_dims, num_dimensions);
    CALI_DATATRACKER_TRACK_DIMENSIONAL(matB, sizeof(double), B_dims, num_dimensions);
    CALI_DATATRACKER_TRACK_DIMENSIONAL(matC, sizeof(double), C_dims, num_dimensions);

    CALI_MARK_END("allocate");
    CALI_MARK_BEGIN("initialize values");

    // Initialize A and B randomly
    for (i = 0; i < M; i++)
        for(k = 0; k < W; k++)
            matA[row_major(i,k,M)] = rand();

    for(k = 0; k < W; k++)
        for(j = 0; j < N; j++)
            matB[row_major(k,j,W)] = rand();

    CALI_MARK_END("initialize values");
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

    std::cout << "cSum = " << cSum << std::endl;

    CALI_MARK_END("sum");
    CALI_MARK_BEGIN("free");

    CALI_DATATRACKER_FREE(matA);
    CALI_DATATRACKER_FREE(matB);
    CALI_DATATRACKER_FREE(matC);

    CALI_MARK_END("free");
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

    CALI_MARK_BEGIN("benchmark");
    
    CALI_CXX_MARK_LOOP_BEGIN(loop, "loop");
    for(size_t i=0; i<num_iterations; i++) {
        CALI_CXX_MARK_LOOP_ITERATION(loop, i);
        do_work(m_size, w_size, n_size);
    }
    CALI_CXX_MARK_LOOP_END(loop);

    CALI_MARK_END("benchmark");
}
