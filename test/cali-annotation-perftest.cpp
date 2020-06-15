// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

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

#include <caliper/Caliper.h>
#include <caliper/ChannelController.h>
#include <caliper/ConfigManager.h>

#include <caliper/common/RuntimeConfig.h>

#include <caliper/cali.h>
#include <caliper/tools-util/Args.h>

#include <chrono>
#include <iostream>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef CALIPER_HAVE_ADIAK
#include <adiak.hpp>
#endif

cali::Annotation         test_annotation("test.attr", CALI_ATTR_SCOPE_THREAD);
std::vector<std::string> annotation_strings;

struct Config
{
    int tree_width;
    int tree_depth;

    int iter;

    int channels;
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

void make_strings(const Config& cfg)
{
    CALI_CXX_MARK_FUNCTION;

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

void record_globals(const Config& cfg, int threads, const cali::ConfigManager::argmap_t& extra_kv)
{
#ifdef CALIPER_HAVE_ADIAK
    adiak::value("perftest.tree_width", cfg.tree_width);
    adiak::value("perftest.tree_depth", cfg.tree_depth);
    adiak::value("perftest.iterations", cfg.iter);
    adiak::value("perftest.threads",    threads);
    adiak::value("perftest.channels",   cfg.channels);

    adiak::value("perftest.services",
                 cali::RuntimeConfig::get_default_config().get("services", "enable").to_string());

    adiak::user();
    adiak::launchdate();
    // adiak::executablepath();
    // adiak::libraries();
    adiak::cmdline();
    adiak::clustername();
    adiak::hostname();

    for (auto &p : extra_kv)
        adiak::value(p.first, p.second);
#else
    cali_set_global_int_byname("perftest.tree_width", cfg.tree_width);
    cali_set_global_int_byname("perftest.tree_depth", cfg.tree_depth);
    cali_set_global_int_byname("perftest.iterations", cfg.iter);
    cali_set_global_int_byname("perftest.threads",    threads);
    cali_set_global_int_byname("perftest.channels",   cfg.channels);
#endif
}

int main(int argc, char* argv[])
{
    cali_config_preset("CALI_ATTRIBUTE_DEFAULT_SCOPE", "process");

    const util::Args::Table option_table[] = {
        { "width",       "tree-width",  'w', true,
          "Context tree width", "WIDTH"
        },
        { "depth",       "tree-width",  'd', true,
          "Context tree depth", "DEPTH"
        },
        { "iterations",  "iterations",  'i', true,
          "Iterations",         "ITERATIONS"
        },
        { "csv",         "print-csv",   'c', false,
          "CSV output. Fields: Tree depth, tree width, number of updates, threads, total runtime.",
          nullptr
        },
        { "channels",     "channels",   'x', true,
          "Number of replicated channel instances",
          "CHANNELS"
        },
        { "profile",       "profile",   'P', true,
          "Caliper profiling config (for profiling cali-annotation-perftest)",
          "CONFIGSTRING"
        },

        { "quiet", "quiet", 'q', false, "Don't print output", nullptr },
        { "help",  "help",  'h', false, "Print help",         nullptr },

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

    cali::ConfigManager::argmap_t extra_kv;

    cali::ConfigManager mgr;
    mgr.set_default_parameter("aggregate_across_ranks", "false");
    mgr.add(args.get("profile", "").c_str(), extra_kv);

    if (mgr.error())
        std::cerr << "Profiling config error: " << mgr.error_msg() << std::endl;

    mgr.start();

    CALI_MARK_FUNCTION_BEGIN;

    int threads = 1;
#ifdef _OPENMP
    threads = omp_get_max_threads();
#endif

    Config cfg;

    cfg.tree_width  = std::stoi(args.get("width", "20"));
    cfg.tree_depth  = std::stoi(args.get("depth", "10"));
    cfg.iter        = std::stoi(args.get("iterations", "100000"));
    cfg.channels    = std::max(std::stoi(args.get("channels", "1")), 1);

    // set global attributes before other Caliper initialization
    record_globals(cfg, threads, extra_kv);

    make_strings(cfg);

    // --- print info

    bool print_csv = args.is_set("csv");
    bool quiet     = args.is_set("quiet");

    if (!quiet && !print_csv)
        std::cout << "cali-annotation-perftest:"
                  << "\n    Channels:   " << cfg.channels
                  << "\n    Tree width: " << cfg.tree_width
                  << "\n    Tree depth: " << cfg.tree_depth
                  << "\n    Iterations: " << cfg.iter
#ifdef _OPENMP
                  << "\n    Threads:    " << omp_get_max_threads()
#endif
                  << std::endl;

    // --- create channels (replicate given user config)

    cali::Caliper c;

    for (int x = 1; x < cfg.channels; ++x) {
        std::string s("chn.");
        s.append(std::to_string(x));

        c.create_channel(s.c_str(), cali::RuntimeConfig::get_default_config());
    }

    // --- pre-timing loop. initializes OpenMP subsystem

    CALI_MARK_BEGIN("perftest.pre-timing");

    Config pre_cfg;

    pre_cfg.tree_width = 1;
    pre_cfg.tree_depth = 0;
    pre_cfg.iter       = 100 * threads;

    mgr.stop();
    run(pre_cfg);
    mgr.start();

    CALI_MARK_END("perftest.pre-timing");

    // --- timing loop

    CALI_MARK_BEGIN("perftest.timing");

    // stop the annotation profiling channels for the measurement loop
    mgr.stop();

    auto stime = std::chrono::system_clock::now();

    int updates = run(cfg);

    auto etime = std::chrono::system_clock::now();

    // re-start the annotation profiling channels
    mgr.start();

    CALI_MARK_END("perftest.timing");

    auto msec  = std::chrono::duration_cast<std::chrono::milliseconds>(etime-stime).count();

    double usec_per_update = (updates > 0 ? (1000.0*msec*threads)/updates : 0.0);
    double updates_per_sec = (msec    > 0 ?  1000.0*updates/msec          : 0.0);

#ifdef CALIPER_HAVE_ADIAK
    adiak::value("perftest.usec_per_update", usec_per_update);
    adiak::value("perftest.updates_per_sec", updates_per_sec);
    adiak::value("perftest.time", msec / 1000.0);
#endif

    if (!quiet) {
        if (print_csv)
            std::cout << cfg.channels
                    << "," << cfg.tree_depth
                    << "," << cfg.tree_width
                    << "," << updates
                    << "," << threads
                    << "," << msec/1000.0
                    << std::endl;
        else
            std::cout << "  " << updates << " annotation updates in "
                    << msec/1000.0     << " sec ("
                    << updates/threads << " per thread), "
                    << updates_per_sec << " updates/sec, "
                    << usec_per_update << " usec/update"
                    << std::endl;
    }

    CALI_MARK_FUNCTION_END;

    mgr.flush();

    return 0;
}
