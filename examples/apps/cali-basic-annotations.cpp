// Copyright (c) 2015, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// A C++ Caliper instrumentation demo

//   Usage: $ cali-basic-annotations <configuration-string>
// e.g. $ cali-basic-annotations "runtime-report,event-trace(output=trace.cali)"

#include <caliper/cali.h>
#include <caliper/cali-manager.h>

#include <cstring>
#include <iostream>
#include <iterator>

int main(int argc, char* argv[])
{
    cali::ConfigManager mgr;

    // Read configuration string from command-line argument
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            std::cerr << "Usage: cali-basic-annotations [caliper-config(arg=...,),...]."
                      << " Available configurations: ";            

            for (auto str : cali::ConfigManager::get_config_docstrings())
                std::cerr << "\n  " << str;

            std::cerr << std::endl;

            return 0;
        } else if (strcmp(argv[1], "--list-configs") == 0) {
            std::cerr << "Available Caliper configurations: ";

            int c = 0;
            for (auto str : cali::ConfigManager::available_configs())
                std::cerr << (c++ > 0 ? "," : "") << str;
            
            std::cerr << std::endl;
            return 0;
        } else {
            mgr.add(argv[1]);

            if (mgr.error())
                std::cerr << "Caliper config error: " << mgr.error_msg() << std::endl;
        }
    }

    //   Get all requested Caliper configuration channel controllers,
    // and start them. Should be done prior to the first region annotations.
    auto channels = mgr.get_all_channels();

    for (auto &c : channels)
        c->start();

    // Mark begin/end of the current function.
    //   Sets "function=main" in Caliper.
    CALI_MARK_FUNCTION_BEGIN;

    // Mark begin and end of a code region.
    //   Sets "annotation=init" in Caliper.
    CALI_MARK_BEGIN("init");

    int    count = 4;
    double t = 0, delta_t = 0.42;

    CALI_MARK_END("init");

    // Mark begin and end of a loop.
    //   Sets "loop=mainloop" in Caliper.
    CALI_CXX_MARK_LOOP_BEGIN(mainloop, "main loop");

    for (int i = 0; i < count; ++i) {
        // Mark each loop iteration of an annotated loop.
        //   Sets "iteration#main loop=<i> in Caliper."
        CALI_CXX_MARK_LOOP_ITERATION(mainloop, i);

        // A Caliper snapshot taken at this point will contain
        // { "function"="main", "loop"="main loop", "iteration#main loop"=<i> }

        t += delta_t;
    }

    CALI_CXX_MARK_LOOP_END(mainloop);

    CALI_MARK_FUNCTION_END;

    // Trigger output in all configuration channels
    for (auto &c : channels)
        c->flush();
}
