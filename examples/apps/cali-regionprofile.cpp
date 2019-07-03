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

    // Get and print the inclusive time spent in each region
    {
        std::map<std::string, double> region_times;
        double total_time;
        
        std::tie(region_times, std::ignore, total_time) =
            rp.inclusive_region_times();

        std::cerr << "Inclusive time per region:"
                  << "\n  main:       " << region_times["main"]
                  << "\n    init:     " << region_times["init"]
                  << "\n    mainloop: " << region_times["mainloop"]
                  << "\n      foo:    " << region_times["foo"]
                  << "\n(Total profiling time: " << total_time << " sec)\n"
                  << std::endl;
    }

    // Get and print the exclusive time spent in each region
    {
        std::map<std::string, double> region_times;
        double total_time;
        
        std::tie(region_times, std::ignore, total_time) =
            rp.exclusive_region_times();

        std::cerr << "Exclusive time per region:"
                  << "\n  main:       " << region_times["main"]
                  << "\n    init:     " << region_times["init"]
                  << "\n    mainloop: " << region_times["mainloop"]
                  << "\n      foo:    " << region_times["foo"]
                  << "\n(Total profiling time: " << total_time << " sec)\n"
                  << std::endl;
    }

    // Get and print the exclusive time in function regions
    {
        std::map<std::string, double> region_times;
        double total_function_time;
        double total_time;
        
        std::tie(region_times, total_function_time, total_time) =
            rp.exclusive_region_times("function");

        std::cerr << "Exclusive time per region (functions only):"
                  << "\n  main:       " << region_times["main"]
                  << "\n    foo:      " << region_times["foo"]
                  << "\n(Total exclusive time in functions: " << total_function_time
                  << " of " << total_time << " sec)"
                  << std::endl;
    }    
}
