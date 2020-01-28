// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// A minimal Caliper function object demo 

#include <caliper/cali.h>
#include <caliper/CaliFunctional.h>

// This example shows how to instrument an application using Caliper's
// instrumented function objects
//
// This interface allows for simple instrumentation of functions and their
// arguments via C++ functors

// Initial code might be like the following comment

/**
 
int sum(int x, int y){
  return x + y;
}

*/

// To instrument, instead first rename your function to "wrapped"

int wrapped_sum(int x, int y){
  return x + y;
}

// Then make a function object with the original name
// by calling cali::wrap_function_and_args, which takes
// in the name you want that function identified by
// and the function to wrap.
//
// In this case we want to name the function "sum" and
// create it by wrapping "wrapped_sum"
auto sum = cali::wrap_function_and_args("sum",wrapped_sum);

// You can wrap anything you can get a handle to, see:
auto wrapped_malloc = cali::wrap_function_and_args("malloc",malloc); 

// Note that you don't have to profile arguments and return values:
auto minimally_wrapped_free = cali::wrap_function("free",free);

int main(int argc, char* argv[])
{
    int seven = sum(3,4);
    int* int_pointer = (int*)wrapped_malloc(sizeof(int)* 100);
    minimally_wrapped_free(int_pointer);
}
