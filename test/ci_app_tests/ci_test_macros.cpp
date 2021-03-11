// --- Caliper continuous integration test app for basic trace test

#define _XOPEN_SOURCE
#include <unistd.h> /* usleep */

#include "caliper/cali.h"
#include "caliper/cali-manager.h"

#include <string>

// test C and C++ macros

void foo(int count, int sleep_usec)
{
    CALI_CXX_MARK_FUNCTION;

    CALI_MARK_BEGIN("pre-loop");
    CALI_WRAP_STATEMENT("foo.init", count = std::max(1, count));
    CALI_MARK_END("pre-loop");

    CALI_MARK_LOOP_BEGIN(fooloop, "fooloop");
    for (int i = 0; i < count; ++i) {
        CALI_MARK_ITERATION_BEGIN(fooloop, i);

        if (sleep_usec > 0)
            usleep(sleep_usec);

        CALI_MARK_ITERATION_END(fooloop);
    }
    CALI_MARK_LOOP_END(fooloop);
}

int main(int argc, char* argv[])
{
    int sleep_usec = 0;

    if (argc > 1)
        sleep_usec = std::stoi(argv[1]);

    cali::ConfigManager mgr;

    if (argc > 2)
        if (std::string(argv[2]) != "none")
            mgr.add(argv[2]);

    if (mgr.error()) {
        std::cerr << mgr.error_msg() << std::endl;
        return -1;
    }

    mgr.start();

    CALI_MARK_FUNCTION_BEGIN;

    int count = 4;

    {
        CALI_CXX_MARK_SCOPE("before_loop");

        if (argc > 3)
            count = std::max(1, std::stoi(argv[3]));
    }

    CALI_CXX_MARK_LOOP_BEGIN(mainloop, "main loop");

    for (int i = 0; i < count; ++i) {
        CALI_CXX_MARK_LOOP_ITERATION(mainloop, i);

        foo(count, sleep_usec);
    }

    CALI_CXX_MARK_LOOP_END(mainloop);
    CALI_MARK_FUNCTION_END;

    mgr.flush();
}
