// Copyright (c) 2015, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by Alfredo Gimenez, gimenez1@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


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
