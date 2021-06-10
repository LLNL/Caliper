#include <caliper/cali.h>
#include <caliper/cali-manager.h>

#include <omp.h>

#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[])
{
    cali_config_set("CALI_CALIPER_ATTRIBUTE_DEFAULT_SCOPE", "process");

    cali::ConfigManager mgr;

    if (argc > 1)
        mgr.add(argv[1]);
    if (mgr.error()) {
        std::cerr << mgr.error_msg() << std::endl;
        return EXIT_FAILURE;
    }

    mgr.start();

    CALI_MARK_FUNCTION_BEGIN;

    omp_set_num_threads(2);

    int sum = 0;

    #pragma omp parallel for reduction(+:sum)
    for (int i = 0; i < 42; ++i) {
        sum += i;
    }

    // use value to prevent loop elision
    cali_set_global_int_byname("ci_test_openmp.result", sum);

    CALI_MARK_FUNCTION_END;

    mgr.flush();
}