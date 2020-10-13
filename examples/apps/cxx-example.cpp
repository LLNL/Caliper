// Copyright (c) 2015-2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// A C++ Caliper instrumentation and ConfigManager example

//   Usage: $ cali-basic-annotations <configuration-string>
// For example, "$ cali-basic-annotations runtime-report" will print a
// hierarchical runtime summary for all annotated regions.

#include <caliper/cali.h>
#include <caliper/cali-manager.h>

#include <cstring>
#include <iostream>
#include <string>

void print_help(const cali::ConfigManager& mgr)
{
    std::cerr << "Usage: cxx-example [caliper-config(arg=...,),...]."
              << "\nRuns \"runtime-report\" configuration by default."
              << "\nUse \"none\" to run without a ConfigManager configuration."
              << "\nAvailable configurations: ";

    auto configs = mgr.available_config_specs();

    // Print info on all available ConfigManager configurations.
    for (auto str : configs)
        std::cerr << "\n" << mgr.get_documentation_for_spec(str.c_str());

    std::cerr << std::endl;
}

double foo(int i)
{
    //   A function annotation. Opens region "function=foo" in Caliper,
    // and automatically closes it at the end of the function.
    CALI_CXX_MARK_FUNCTION;

    return 0.5*i;
}

int main(int argc, char* argv[])
{
    //   The ConfigManager manages built-in or custom Caliper measurement
    // configurations, and provides an API to control performance profiling.
    cali::ConfigManager mgr;

    //   Use the "runtime-report" configuration by default to print a
    // runtime summary for all annotated regions. Let users overwrite
    // the default configuration with their own on the command line.
    std::string configstr = "runtime-report";

    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_help(mgr);
            return 0;
        } else {
            configstr = argv[1];
        }
    }

    if (configstr == "none")
        configstr.clear();

    //   Enable the requested performance measurement channels and start
    // profiling.
    mgr.add(configstr.c_str());

    if (mgr.error())
        std::cerr << "Caliper config error: " << mgr.error_msg() << std::endl;

    mgr.start();

    //   Mark begin of the current function. Must be manually closed.
    // Opens region "function=main" in Caliper.
    CALI_MARK_FUNCTION_BEGIN;

    // Mark a code region. Opens region "annotation=init" in Caliper.
    CALI_MARK_BEGIN("init");
    int count = 4;
    double  t = 0;
    CALI_MARK_END("init");

    // Mark a loop. Opens region "loop=mainloop" in Caliper.
    CALI_CXX_MARK_LOOP_BEGIN(loop_ann, "mainloop");

    for (int i = 0; i < count; ++i) {
        //   Mark loop iterations of an annotated loop.
        // Sets "iteration#main loop=<i> in Caliper.
        CALI_CXX_MARK_LOOP_ITERATION(loop_ann, i);

        //   A Caliper snapshot taken at this point will contain
        // { "function"="main", "loop"="mainloop", "iteration#main loop"=<i> }

        t += foo(i);
    }

    // Mark the end of the "loop=mainloop" region.
    CALI_CXX_MARK_LOOP_END(loop_ann);
    // Mark the end of the "function=main" region.
    CALI_MARK_FUNCTION_END;

    //   Trigger output in all Caliper control channels.
    // This should be done after all measurement regions have been closed.
    mgr.flush();
}
