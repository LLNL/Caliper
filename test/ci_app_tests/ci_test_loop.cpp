#include <caliper/cali.h>

#include <string>

void foo()
{
    CALI_CXX_MARK_FUNCTION;
}

int main(int argc, char* argv[])
{
    CALI_CXX_MARK_FUNCTION;
    
    size_t loopcount = 1000;
    
    if (argc > 1)
        loopcount = std::stoul(argv[1]);

    CALI_CXX_MARK_LOOP_BEGIN(loop, "loop");

    for (size_t c = 0; c < loopcount; ++c) {
        CALI_CXX_MARK_LOOP_ITERATION(loop, c);
        foo();
    }

    CALI_CXX_MARK_LOOP_END(loop);
}
