// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// -- cali-flush-perftest
//
// Runs a performance test for caliper flush operations.
//
// The benchmark runs an annotated loop to fill up trace buffers or
// aggregation buffers, then triggers a flush.
//
// The benchmark is multi-threaded: the loop is statically divided
// between threads using OpenMP.

#include <caliper/common/Attribute.h>
#include <caliper/common/RuntimeConfig.h>

#include <caliper/Caliper.h>

#include <caliper/cali.h>
#include <caliper/tools-util/Args.h>

#include <chrono>
#include <iostream>

#ifdef _OPENMP
#include <omp.h>
#endif

struct Config
{
    int  iter;
    int  nxtra;
    bool write;
    
    std::vector<cali::Attribute> xtra_attrs;

    int channels;
};

void run(const Config& cfg)
{
#pragma omp parallel
    {
        CALI_CXX_MARK_LOOP_BEGIN(testloop, "testloop");
        
#pragma omp for schedule(static)
        for (int i = 0; i < cfg.iter; ++i) {
            CALI_CXX_MARK_LOOP_ITERATION(testloop, i);

            cali::Caliper c;

            for (int x = 0, div = 10; x < cfg.nxtra; ++x, div *= 10)
                c.begin(cfg.xtra_attrs[x], cali::Variant(i/div));
            for (int x = 0; x < cfg.nxtra; ++x)
                c.end(cfg.xtra_attrs[cfg.nxtra-(1+x)]);
        }

        CALI_CXX_MARK_LOOP_END(testloop);
    }
}


int main(int argc, char* argv[])
{
    cali_config_preset("CALI_CALIPER_ATTRIBUTE_PROPERTIES", "annotation=nested:process_scope");
    cali_config_set("CALI_CHANNEL_FLUSH_ON_EXIT", "false");

    const util::Args::Table option_table[] = {
        { "iterations", "iterations", 'i', true,
          "Number of loop iterations", "ITERATIONS"   },
        { "xtra",       "xtra",       'x', true,
          "Number of extra attributes", "XTRA"   },
        
        { "channels",   "channels",   'c', true,
          "Number of replicated channels", "CHANNELS" },

        { "write", "write", 'w', false,
          "Write to output service in addition to flush", nullptr },

        { "help", "help", 'h', false, "Print help", nullptr },

        util::Args::Table::Terminator
    };

    // --- Initialization

    util::Args args(option_table);

    int lastarg = args.parse(argc, argv);

    if (lastarg < argc) {
        std::cerr << "cali-flush-perftest: unknown option: " << argv[lastarg] << '\n'
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

    cfg.iter     = std::stoi(args.get("iterations", "100000"));
    cfg.nxtra    = std::stoi(args.get("xtra",       "2"));
    cfg.channels = std::stoi(args.get("channels",   "1"));
    cfg.write    = args.is_set("write"); 
        
    cali_set_global_int_byname("flush-perftest.iterations", cfg.iter);
    cali_set_global_int_byname("flush-perftest.nxtra", cfg.nxtra);
    cali_set_global_int_byname("flush-perftest.channels", cfg.channels);
    cali_set_global_int_byname("flush-perftest.threads", threads);

    // --- print info

    std::cout << "cali-flush-perftest:"
              << "\n    Channels:   " << cfg.channels
              << "\n    Iterations: " << cfg.iter
              << "\n    Xtra:       " << cfg.nxtra
#ifdef _OPENMP
              << "\n    Threads:    " << threads
#endif
              << std::endl;
    
    // --- set up the xtra attributes
    
    cali::Caliper c;
    
    for (int i = 0, div = 10; i < cfg.nxtra; ++i, div *= 10)
        cfg.xtra_attrs.push_back(c.create_attribute(std::string("x.")+std::to_string(div),
                                                    CALI_TYPE_INT,
                                                    CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE));

    // --- set up channels

    std::vector<cali::Channel*> channels;
    channels.push_back(c.get_channel(0));

    for (int i = 1; i < cfg.channels; ++i) {
        std::string s("chn.");
        s.append(std::to_string(i));

        channels.push_back(c.create_channel(s.c_str(), cali::RuntimeConfig::get_default_config()));
    }

    // --- Run the loop to fill buffers

    CALI_MARK_BEGIN("fill");

    run(cfg);

    CALI_MARK_END("fill");
    
    // --- Timed flush

    int snapshots = 3 + cfg.channels * (2*threads + (cfg.iter * (2 + 2*cfg.nxtra)));
    
    CALI_MARK_BEGIN("flush");

    auto stime = std::chrono::system_clock::now();

    for (cali::Channel* chn : channels)
        if (cfg.write)
            c.flush_and_write(chn, nullptr);
        else
            c.flush(chn, nullptr, [](cali::CaliperMetadataAccessInterface&, const std::vector<cali::Entry>&) { });

    auto etime = std::chrono::system_clock::now();

    CALI_MARK_END("flush");

    auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(etime-stime).count();

    std::cout << "  " << snapshots     << " snapshots flushed in "
              << msec/1000.0           << " sec, "
              << 1000.0*msec/snapshots << " usec/snapshot"
              << std::endl;
}
