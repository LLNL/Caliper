// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// A minimal Caliper instrumentation demo 

#include <caliper/Annotation.h>
#include <math.h>
#include <caliper/CaliFunctional.h>

int doWork(int* inArray, int size){
    int reduce_var = 0;
    for(int i=0; i<size; i++){
        inArray[i] = i*i;
        reduce_var += i*i;
    }
    return  reduce_var;
}

template<typename T>
T* initialize(size_t data_size, T initial_value){
    T* array = (T*)malloc(sizeof(T)*data_size);
    for(size_t i=0;i<data_size;i++){
        array[i] = initial_value;
    }
    return array;
}

auto doWorkWrapped = cali::wrap_function_and_args("doWork", doWork);



int main(int argc, char* argv[])
{
    constexpr int data_size = 1000000;
    constexpr int iterations = 10;
    int data_size_increment = data_size/iterations;
    cali::wrap("Program",[&](){
        int * inArray;
        cali::wrap("Initialization",[&](){
            inArray = cali::wrap_with_args("initializer",initialize<int>,data_size, 0);
        });
        for(int size = data_size_increment; size<=data_size; size+=data_size_increment){
            doWorkWrapped(inArray,size);
        }
    });
}
