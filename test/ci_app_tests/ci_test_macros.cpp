// --- Caliper continuous integration test app for basic trace test

#include "caliper/cali.h"

// test C macros

void foo()
{
    CALI_MARK_FUNCTION_BEGIN;

    CALI_MARK_BEGIN("pre-loop");
    CALI_WRAP_STATEMENT("foo.init", int count = 4);
    CALI_MARK_END("pre-loop");

    CALI_MARK_LOOP_BEGIN(fooloop, "fooloop");
    for (int i = 0; i < count; ++i) {
        CALI_MARK_ITERATION_BEGIN(fooloop, i);

        CALI_MARK_ITERATION_END(fooloop);
    }
    CALI_MARK_LOOP_END(fooloop);

    CALI_MARK_FUNCTION_END;
}

int main() 
{
    CALI_CXX_MARK_FUNCTION;
    
    const int count = 4;

    CALI_CXX_MARK_LOOP_BEGIN(mainloop, "mainloop");

    for (int i = 0; i < count; ++i) {
        CALI_CXX_MARK_LOOP_ITERATION(mainloop, i);

        foo();
    }
    
    CALI_CXX_MARK_LOOP_END(mainloop);
}
