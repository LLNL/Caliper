// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include <iostream>
#include <cstdlib>
#include <algorithm>

#include <caliper/cali.h>
#include <caliper/cali_datatracker.h>

int* init(size_t arraySize, bool sort)
{
    CALI_CXX_MARK_FUNCTION;

    int *data = 
        static_cast<int*>(cali_datatracker_allocate_dimensional("data", sizeof(int), &arraySize, 1));

    std::srand(1337);
    for (size_t c = 0; c < arraySize; ++c)
        data[c] = std::rand() % 256;

    if (sort)
        std::sort(data, data + arraySize); 

    return data;
}

void work(int *data, size_t arraySize)
{
    CALI_CXX_MARK_FUNCTION;

    long sum = 0;

    for (size_t i = 0; i < 100000; ++i)
    {
        // Primary loop
        for (size_t c = 0; c < arraySize; ++c)
        {
            if (data[c] >= 128)
                sum += data[c];
        }
    }
    std::cout << "sum = " << sum << std::endl;
}

void cleanup(int *data)
{
    CALI_CXX_MARK_FUNCTION;

    cali_datatracker_free(data);
}

void benchmark(size_t arraySize, bool sort)
{
    CALI_CXX_MARK_FUNCTION;

    cali::Annotation sorted("sorted");
    sorted.set(sort);

    std::cout << "Initializing benchmark data with sort = " << sort << std::endl;

    int *data = init(arraySize, sort);

    std::cout << "Calculating sum of values >= 128" << std::endl;

    work(data, arraySize);

    std::cout << "Cleaning up" << std::endl;

    cleanup(data);

    std::cout << "Done!" << std::endl;
}

int main(int argc, char *argv[])
{
    CALI_CXX_MARK_FUNCTION;

    // Generate data
    size_t arraySize = 32768;
    benchmark(arraySize, true);
    benchmark(arraySize, false);
}
