// Copyright (c) 2015-2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// A C Caliper instrumentation and ConfigManager example

//   Usage: $ cali-basic-annotations <configuration-string>
// For example, "$ cali-basic-annotations runtime-report" will print a
// hierarchical runtime summary for all annotated regions.

#include <caliper/cali.h>
#include <caliper/cali-manager.h>

#include <stdio.h>
#include <string.h>

void print_help()
{
    const char* helpstr = 
        "Usage: c-example [caliper-config(arg=...,),...]."
        "\nRuns \"runtime-report\" configuration by default."
        "\nUse \"none\" to run without a ConfigManager configuration."
        "\nAvailable configurations: ";

    puts(helpstr);
}

double foo(int i)
{
    //   A function annotation. Opens region "function=foo" in Caliper,
    // and automatically closes it at the end of the function.
    CALI_MARK_FUNCTION_BEGIN;
    double res = 0.5 * i;
    CALI_MARK_FUNCTION_END;
    return res;
}

int main(int argc, char* argv[])
{
    //   The ConfigManager manages built-in or custom Caliper measurement
    // configurations, and provides an API to control performance profiling.
    cali_ConfigManager mgr;
    cali_ConfigManager_new(&mgr);

    //   We can set default parameters for the Caliper configurations.
    // These can be overridden in the user-provided configuration string.
    cali_ConfigManager_set_default_parameter(&mgr, "aggregate_across_ranks", "false");

    //   Use the "runtime-report" configuration by default to print a
    // runtime summary for all annotated regions. Let users overwrite
    // the default configuration with their own on the command line.
    const char* configstr = "runtime-report";

    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_help();
            return 0;
        } else {
            configstr = argv[1];
        }
    }

    if (strcmp(configstr, "none") == 0)
        configstr = "";

    //   Enable the requested performance measurement channels and start
    // profiling.
    cali_ConfigManager_add(&mgr, configstr);

    if (cali_ConfigManager_error(&mgr)) {
        cali_SHROUD_array errmsg;
        cali_ConfigManager_error_msg_bufferify(&mgr, &errmsg);
        fprintf(stderr, "Caliper config error: %s\n", errmsg.addr.ccharp);
        cali_SHROUD_memory_destructor(&errmsg.cxx);
    }

    cali_ConfigManager_start(&mgr);

    //   Mark begin of the current function. Must be manually closed.
    // Opens region "function=main" in Caliper.
    CALI_MARK_FUNCTION_BEGIN;

    // Mark a code region. Opens region "annotation=init" in Caliper.
    CALI_MARK_BEGIN("init");
    int count = 4;
    double  t = 0;
    CALI_MARK_END("init");

    // Mark a loop. Opens region "loop=mainloop" in Caliper.
    CALI_MARK_LOOP_BEGIN(loop_ann, "mainloop");

    for (int i = 0; i < count; ++i) {
        //   Mark loop iterations of an annotated loop.
        // Sets "iteration#main loop=<i> in Caliper.
        CALI_MARK_ITERATION_BEGIN(loop_ann, i);

        //   A Caliper snapshot taken at this point will contain
        // { "function"="main", "loop"="mainloop", "iteration#main loop"=<i> }

        t += foo(i);

        CALI_MARK_ITERATION_END(loop_ann);
    }

    // Mark the end of the "loop=mainloop" region.
    CALI_MARK_LOOP_END(loop_ann);
    // Mark the end of the "function=main" region.
    CALI_MARK_FUNCTION_END;

    //   Trigger output in all Caliper control channels.
    // This should be done after all measurement regions have been closed.
    cali_ConfigManager_flush(&mgr);
    cali_ConfigManager_delete(&mgr);
}
