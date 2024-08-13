// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// A C Caliper instrumentation and ConfigManager example

//   Usage: $ cali-basic-annotations <configuration-string>
// For example, "$ cali-basic-annotations runtime-report" will print a
// hierarchical runtime summary for all annotated regions.

#include <caliper/cali.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define LARGE_NUM 100000000

void foo()
{
    printf("Enter Foo. Run a long spinloop\n");
    CALI_MARK_FUNCTION_BEGIN;
    long double res=0.1;
    int i;
    for (i=0;i<1000000000;i++)
        res += res * i;
    CALI_MARK_FUNCTION_END;
    printf("Exit Foo. \n");
}

int main(int argc, char* argv[])
{
    // Mark begin of the current function. Must be manually closed.
    CALI_MARK_FUNCTION_BEGIN;
    printf("Hello World\n");
    foo();
    // Mark the end of the current function
    CALI_MARK_FUNCTION_END;
}
