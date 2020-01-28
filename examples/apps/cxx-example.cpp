// Copyright (c) 2015-2019, Lawrence Livermore National Security, LLC.
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

void print_help()
{
    std::cerr << "Usage: cxx-example [caliper-config(arg=...,),...]."
              << "\nRuns \"runtime-report\" configuration by default."
              << "\nUse \"none\" to run without a ConfigManager configuration."
              << "\nAvailable configurations: ";

    // Print info on all available ConfigManager configurations.
    for (auto str : cali::ConfigManager::get_config_docstrings())
        std::cerr << '\n' << str;

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
    //   A Caliper ConfigManager object manages built-in Caliper measurement
    // configurations. It parses a configuration string and creates a set of
    // control channels for the requested measurement configurations.
    cali::ConfigManager mgr;

    //   We can set default parameters for the Caliper configurations.
    // These can be overridden in the user-provided configuration string.
    mgr.set_default_parameter("aggregate_across_ranks", "false");

    //   The "runtime-report" configuration will print a hierarchical
    // runtime summary for all annotated regions.
    std::string configstr = "runtime-report";

    //   Let users overwrite the default configuration with their own
    // on the command line.
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_help();
            return 0;
        } else {
            configstr = argv[1];
        }
    }

    if (configstr == "none")
        configstr.clear();

    //   Configure Caliper performance measurement channels based on
    // the configuration string.
    mgr.add(configstr.c_str());

    //   Check if the configuration string was parsed successfully.
    // If not, print an error message.
    if (mgr.error())
        std::cerr << "Caliper config error: " << mgr.error_msg() << std::endl;

    //   Start all requested Caliper measurement control channels.
    // Should be done prior to the first region annotations.
    mgr.start();

    //   Mark begin of the current function. Must be manually closed.
    // Opens region "function=main" in Caliper.
    CALI_MARK_FUNCTION_BEGIN;

    // Mark a code region. Opens region "annotation=init" in Caliper.
    CALI_MARK_BEGIN("init");
    int count = 4;
    double  t = 0;
    CALI_MARK_END("init");

    // Mark a loop. Opens region "loop=main loop" in Caliper.
    CALI_CXX_MARK_LOOP_BEGIN(mainloop, "main loop");

    for (int i = 0; i < count; ++i) {
        //   Mark loop iterations of an annotated loop.
        // Sets "iteration#main loop=<i> in Caliper.
        CALI_CXX_MARK_LOOP_ITERATION(mainloop, i);

        //   A Caliper snapshot taken at this point will contain
        // { "function"="main", "loop"="main loop", "iteration#main loop"=<i> }

        t += foo(i);
    }

    // Mark the end of the "loop=main loop" region.
    CALI_CXX_MARK_LOOP_END(mainloop);
    // Mark the end of the "function=main" region.
    CALI_MARK_FUNCTION_END;

    //   Trigger output in all Caliper control channels.
    // This should be done after all measurement regions have been closed.
    mgr.flush();
}
