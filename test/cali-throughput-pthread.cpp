// 
// Multi-threaded caliper throughput benchmark
//

#include <Args.h>

#include <Annotation.h>
#include <Caliper.h>

#include <Variant.h>

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

struct BenchmarkInfo {
    int  sleeptime;
    int  iterations;
    bool set_thread_id;

    std::vector<cali::Attribute> extra_tree_attr;
    std::vector<cali::Attribute> extra_value_attr;

    void create_extra_tree_attributes(int n) {
        cali::Caliper* c = cali::Caliper::instance();

        for (int i = 0; i < n; ++i) {
            std::stringstream s;
            s << "extra.tree." << i;

            extra_tree_attr.push_back(c->create_attribute(s.str(), CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD));
        }   
    }

    void create_extra_value_attributes(int n) {
        cali::Caliper* c = cali::Caliper::instance();

        for (int i = 0; i < n; ++i) {
            std::stringstream s;
            s << "extra.value." << i;

            extra_value_attr.push_back(c->create_attribute(s.str(), CALI_TYPE_INT, CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD));
        }
    }

    BenchmarkInfo()
        : sleeptime(0), iterations(1), set_thread_id(true)
        { }
};


//
// A thread running through an iteration loop
//

void iteration_throughput_thread(int num, const BenchmarkInfo& info)
{

    cali::Annotation::Guard 
        scope(cali::Annotation("benchmark.threadrun").begin("Thread-local loop"));

    cali::Annotation thread_ann("benchmark.thread.id");

    if (info.set_thread_id)
        thread_ann.set(num);

    cali::Annotation iter_ann("benchmark.loop.iteration");

    const std::string chars { "abcdefghijklmnopqrstuvwxyz0123456789" };

    for (int i = 0; i < info.iterations; ++i) {
        iter_ann.set(i);

        std::string sval = chars.substr((num+i)%(chars.length()/2));
        int64_t     ival = num+i;

        for (auto it = info.extra_tree_attr.begin(); it != info.extra_tree_attr.end(); ++it)
            cali::Caliper::instance()->begin(*it, cali::Variant(CALI_TYPE_STRING, sval.c_str(), sval.length()));
        for (auto it = info.extra_value_attr.begin(); it != info.extra_value_attr.end(); ++it)
            cali::Caliper::instance()->set(*it, cali::Variant(CALI_TYPE_INT, &ival, sizeof(ival)));

        if (info.sleeptime) 
            std::this_thread::sleep_for(std::chrono::microseconds(info.sleeptime));

        for (auto it = info.extra_tree_attr.rbegin(); it != info.extra_tree_attr.rend(); ++it)
            cali::Caliper::instance()->end(*it);
    }

    iter_ann.end();

    if (info.set_thread_id)
        thread_ann.end();
}


int main(int argc, const char* argv[])
{
    // parse command line arguments

    const util::Args::Table option_table[] = {
        { "threads",        "threads",          't', true, 
          "Number of threads",
          "2" },
        { "val-attributes",  "val-attributes",  0,   true, 
          "Number of extra by-value attributes", 
          "2" },
        { "tree-attributes", "tree-attributes", 0,   true, 
          "Number of extra tree attributes",
          "2" },
        { "runs",            "runs",            'r', true,
          "Number of runs",
          "4" },
        { "iterations",      "iterations",      'i', true,
          "Number of iterations",
          "20" },
        { "sleep",           "sleep",           's', true,
          "Sleep time per iteration (in microseconds)",
          "0" },

        util::Args::Table::Terminator
    };

    util::Args args(option_table);

    int lastarg = args.parse(argc, argv);

    if (lastarg < argc) {
        std::cerr << "cali-throughput-thread: unknown option: " << argv[lastarg] << '\n'
                  << "  Available options: ";

        args.print_available_options(std::cerr);

        return -1;
    }

    int num_threads = std::stoi(args.get("threads",         "2"));
    int num_runs    = std::stoi(args.get("runs",            "4"));
    int num_xval    = std::stoi(args.get("val-attributes",  "2"));
    int num_xtree   = std::stoi(args.get("tree-attributes", "2"));

    BenchmarkInfo info;

    info.iterations = std::stoi(args.get("iterations", "20"));
    info.sleeptime  = std::stoi(args.get("sleep",      "0"));

    cali::Annotation("benchmark.threads").set(num_threads);
    cali::Annotation("benchmark.iterations").set(info.iterations);
    cali::Annotation("benchmark.sleeptime").set(info.sleeptime);

    cali::Annotation benchmark_annotation("benchmark");
    cali::Annotation benchmark_run("benchmark.run");


    //
    // create attributes
    //

    benchmark_annotation.begin("Create attributes");

    info.create_extra_value_attributes(num_xval);
    info.create_extra_tree_attributes(num_xtree);

    benchmark_annotation.end();

    //
    // run benchmarks
    //
 
    std::vector<std::thread> threads;

    benchmark_annotation.begin("Iteration throughput test");

    auto stime = std::chrono::system_clock::now();

    for (int run = 0; run < num_runs; ++run) {
        cali::Annotation::Guard benchmark_run_scope(benchmark_run.set(run));

        for (int i = 0; i < num_threads; ++i)
            threads.push_back(std::thread(&iteration_throughput_thread, i, info));

        for (auto& t : threads)
            t.join();

        threads.clear();
    }

    auto etime = std::chrono::system_clock::now();

    benchmark_annotation.end();

    std::cout << "Threads: "     << num_threads 
              << "  Runs: "       << num_runs 
              << "  Iterations: " << info.iterations
              << "  Time: " 
              << std::chrono::duration_cast<std::chrono::milliseconds>(etime-stime).count() 
              << "msec" << std::endl;
}
