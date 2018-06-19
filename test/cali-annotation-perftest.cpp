// Copyright (c) 2018, Lawrence Livermore National Security, LLC.  
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
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the disclaimer below.
//
//  * Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the disclaimer (as
//    noted below) in the documentation and/or other materials
//    provided with the distribution.
//
//  * Neither the name of the LLNS/LLNL nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE
// LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
// USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

// -- cali-annotation-perftest
//
// Runs a performance test for caliper annotations. Essentially,
// it creates nested begin/end annotation calls in a loop, and
// reports the execution time of the whole loop.
//
// The benchmark builds up a context tree with a given width and
// depth. In each iteration, the benchmark opens nested annotations up
// to the given tree depth. The tree width specifies how many
// different annotation nodes will be created at each level.
//
// The number of context tree nodes being created is
//   Width x Depth
//
// The total number of annotation updates being executed is
//   2 x Iterations x Depth
//
// The benchmark is multi-threaded: the loop is statically divided
// between threads using OpenMP.

#include <caliper/cali.h>
#include <caliper/tools-util/Args.h>

#include <chrono>
#include <iostream>

#ifdef _OPENMP
#include <omp.h>
#endif


cali::Annotation         test_annotation("test.attr", CALI_ATTR_DEFAULT);
std::vector<std::string> annotation_strings;

struct Config
{
    int tree_width;
    int tree_depth;

    int iter;
};


int foo(int d, int w, const Config& cfg)
{
    if (d <= 0)
        return 0;

    cali::Annotation::Guard
        g_a(test_annotation.begin(annotation_strings[d*cfg.tree_width+w].c_str()));

    return 2 + foo(d-1, w, cfg);
}

int run(const Config& cfg)
{
    int n_updates = 0;

#pragma omp parallel for schedule(static) reduction(+:n_updates)
    for (int i = 0; i < cfg.iter; ++i) {
        n_updates += foo(cfg.tree_depth, i % cfg.tree_width, cfg);
    }
    
    return n_updates;
}

int make_strings(const Config& cfg)
{
    int depth = cfg.tree_depth + 1;
    int width = std::max(1, cfg.tree_width);
    
    annotation_strings.resize(depth * width);

    for (int d = 0; d < depth; ++d)
        for (int w = 0; w < width; ++w) {
            std::string str("foo.");
            
            str.append(std::to_string(d));
            str.append(".");
            str.append(std::to_string(w));
            
            annotation_strings[d*width+w] = std::move(str);
        }
}

int main(int argc, char* argv[])
{
    cali_config_preset("CALI_CALIPER_ATTRIBUTE_PROPERTIES", "annotation=nested:process_scope");
    
    const util::Args::Table option_table[] = {
        { "width",      "tree-width", 'w', true,
          "Context tree width", "WIDTH"
        },
        { "depth",      "tree-width", 'd', true,
          "Context tree depth", "DEPTH"
        },
        { "iterations", "iterations", 'i', true,
          "Iterations",         "ITERATIONS"
        },
        { "csv",        "print-csv",  'c', false,
          "CSV output. Fields: Tree depth, tree width, number of updates, threads, total runtime."
        },

        { "help", "help", 'h', false, "Print help", nullptr },

        util::Args::Table::Terminator
    };
    
    // --- initialization
    
    util::Args args(option_table);

    int lastarg = args.parse(argc, argv);

    if (lastarg < argc) {
        std::cerr << "cali-annotation-perftest: unknown option: " << argv[lastarg] << '\n'
                  << "Available options: ";

        args.print_available_options(std::cerr);

        return 1;
    }

    if (args.is_set("help")) {
        args.print_available_options(std::cerr);
        return 2;
    }

    int threads = 1;
#ifdef _OPENMP
    threads = omp_get_max_threads();
#endif
        
    Config cfg;

    cfg.tree_width = std::stoi(args.get("width", "20"));
    cfg.tree_depth = std::stoi(args.get("depth", "10"));
    cfg.iter       = std::stoi(args.get("iterations", "100000"));
    
    // set global attributes before other Caliper initialization

    cali::Annotation("perftest.tree_width", CALI_ATTR_GLOBAL).set(cfg.tree_width);
    cali::Annotation("perftest.tree_depth", CALI_ATTR_GLOBAL).set(cfg.tree_depth);
    cali::Annotation("perftest.iterations", CALI_ATTR_GLOBAL).set(cfg.iter);
#ifdef _OPENMP
    cali::Annotation("perftest.threads",    CALI_ATTR_GLOBAL).set(threads);
#endif

    make_strings(cfg);

    // --- print info

    bool print_csv = args.is_set("csv");

    if (!print_csv)
        std::cout << "cali-annotation-perftest:"
                  << "\n    Tree width: " << cfg.tree_width
                  << "\n    Tree depth: " << cfg.tree_depth
                  << "\n    Iterations: " << cfg.iter
#ifdef _OPENMP
                  << "\n    Threads:    " << omp_get_max_threads()
#endif
                  << std::endl;

    // --- pre-timing loop. initializes OpenMP subsystem

    CALI_MARK_BEGIN("perftest.pre-timing");

    Config pre_cfg;

    pre_cfg.tree_width = 1;
    pre_cfg.tree_depth = 0;
    pre_cfg.iter       = 100 * threads;

    run(pre_cfg);

    CALI_MARK_END("perftest.pre-timing");

    // --- timing loop

    CALI_MARK_BEGIN("perftest.timing");

    auto stime = std::chrono::system_clock::now();
    
    int updates = run(cfg);
    
    auto etime = std::chrono::system_clock::now();

    CALI_MARK_END("perftest.timing");

    auto msec  = std::chrono::duration_cast<std::chrono::milliseconds>(etime-stime).count();

    if (print_csv)
        std::cout << cfg.tree_depth
                  << "," << cfg.tree_width
                  << "," << updates
                  << "," << threads
                  << "," << msec/1000.0
                  << std::endl;
    else
        std::cout << "  " << updates << " annotation updates in "
                  << msec/1000.0     << " sec ("
                  << updates/threads << " per thread), "
                  << (msec    > 0 ? 1000.0*updates/msec           : 0.0) << " updates/sec, "
                  << (updates > 0 ? (1000.0*msec*threads)/updates : 0.0) << " usec/update"
                  << std::endl;

    return 0;
}
