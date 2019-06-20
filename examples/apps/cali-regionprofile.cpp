// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

// Example of the RegionProfile channel class

#include <caliper/cali.h>

#include <caliper/RegionProfile.h>

void foo()
{
    CALI_CXX_MARK_FUNCTION;
}


int main()
{
    cali_config_preset("CALI_LOG_VERBOSITY", "0");
    
    //   The RegionProfile channel controller computes the total time spent
    // in annotated regions

    cali::RegionProfile rp;

    // Activate recording 
    rp.start();

    CALI_MARK_FUNCTION_BEGIN;

    CALI_MARK_BEGIN("init");
    int count = 4;
    CALI_MARK_END("init");

    CALI_CXX_MARK_LOOP_BEGIN(mainloop, "mainloop");

    for (int i = 0; i < count; ++i)
        foo();

    CALI_CXX_MARK_LOOP_END(mainloop);
    
    CALI_MARK_FUNCTION_END;

    // Stop recording
    rp.stop();

    // Get and print the exclusive time spent in each region
    {
        std::map<std::string, double> region_times;
        double total;
        
        std::tie(region_times, std::ignore, total) =
            rp.exclusive_region_times();

        std::cerr << "Exclusive time per region: \n";
        
        for (auto &p : region_times)
            std::cerr << "  " << p.first << ": " << p.second << " usec ("
                      << p.second / total * 100.0 << "% of total)\n";

        std::cerr << "(Total: " << total << " usec)\n" << std::endl;
    }

    // Get and print the exclusive time in function regions
    {
        std::map<std::string, double> region_times;
        double total_func;
        double total;
        
        std::tie(region_times, total_func, total) =
            rp.exclusive_region_times("function");

        std::cerr << "Exclusive time per region (functions only): \n";
        
        for (auto &p : region_times)
            std::cerr << "  " << p.first << ": " << p.second << " usec" << std::endl;

        std::cerr << "(Total time in functions: " << total_func
                  << " of " << total << " usec)" << std::endl;
    }    
}
