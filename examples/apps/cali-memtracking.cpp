// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include <caliper/cali.h>
#include <caliper/cali_datatracker.h>

#include "../../src/tools/util/Args.h"

#include <numeric>

size_t row_major(size_t x, size_t y, size_t width)
{
    return width * y + x;
}

void do_work(size_t M, size_t W, size_t N)
{
    size_t i, j, k;

    const size_t dimA[] = { M, W };
    const size_t dimB[] = { W, N };
    const size_t dimC[] = { M, N };

    CALI_MARK_BEGIN("allocate");

    double* matA = new double[dimA[0]*dimA[1]];
    double* matB = new double[dimB[0]*dimB[1]];
    double* matC = new double[dimC[0]*dimC[1]];

    cali_datatracker_track_dimensional(matA, "A", sizeof(double), dimA, 2);
    cali_datatracker_track_dimensional(matB, "B", sizeof(double), dimB, 2);
    cali_datatracker_track_dimensional(matC, "C", sizeof(double), dimC, 2);

    CALI_MARK_END("allocate");
    CALI_MARK_BEGIN("initialize_values");

    // Initialize A and B randomly
    for (i = 0; i < M; i++)
        for (k = 0; k < W; k++)
            matA[row_major(i, k, M)] = rand();

    for (k = 0; k < W; k++)
        for (j = 0; j < N; j++)
            matB[row_major(k, j, W)] = rand();

    CALI_MARK_END("initialize_values");
    CALI_MARK_BEGIN("multiply");

    // AB = C
    for (i = 0; i < M; i++)
        for (j = 0; j < N; j++)
            for (k = 0; k < W; k++)
                matC[row_major(i, j, M)] += matA[row_major(i, k, M)] * matB[row_major(k, j, W)];

    CALI_MARK_END("multiply");
    CALI_MARK_BEGIN("sum");

    // Print sum of elems in C
    double cSum = 0;
    for (i = 0; i < M; i++)
        for (j = 0; j < N; j++)
            cSum += matC[row_major(i, j, M)];

    std::cout << "cSum = " << cSum << std::endl;

    CALI_MARK_END("sum");
    CALI_MARK_BEGIN("free");

    cali_datatracker_untrack(matA);
    cali_datatracker_untrack(matB);
    cali_datatracker_untrack(matC);

    delete[] matA;
    delete[] matB;
    delete[] matC;

    CALI_MARK_END("free");
}

int main(int argc, const char* argv[])
{
    CALI_CXX_MARK_FUNCTION;

    // parse command line arguments

    const util::Args::Table option_table[] = {
        { "m_size", "m_size", 'm', true, "Width of input matrix A", "elements" },
        { "w_size", "w_size", 'w', true, "Height of input matrix A and width of input matrix B", "elements" },
        { "n_size", "n_size", 'n', true, "Height of input matrix B", "elements" },
        { "iterations", "iterations", 'i', true, "Number of iterations", "iterations" },

        util::Args::Terminator
    };

    util::Args args(option_table);

    int lastarg = args.parse(argc, argv);

    if (lastarg < argc) {
        std::cerr << "cali-memtracking: unknown option: " << argv[lastarg] << '\n' << "  Available options: ";
        args.print_available_options(std::cerr);
        return -1;
    }

    size_t m_size         = std::stoul(args.get("m_size", "512"));
    size_t w_size         = std::stoul(args.get("w_size", "512"));
    size_t n_size         = std::stoul(args.get("n_size", "512"));
    size_t num_iterations = std::stoul(args.get("iterations", "4"));

    CALI_CXX_MARK_LOOP_BEGIN(loop, "loop");
    for (size_t i = 0; i < num_iterations; i++) {
        CALI_CXX_MARK_LOOP_ITERATION(loop, i);
        do_work(m_size, w_size, n_size);
    }
    CALI_CXX_MARK_LOOP_END(loop);
}
