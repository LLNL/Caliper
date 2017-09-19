#include "perf_postprocessing.h"

#include <linux/perf_event.h>

const std::string s_mem_lvl_na = "Not Available";
const std::string s_mem_lvl_l1 = "L1";
const std::string s_mem_lvl_lfb = "LFB";
const std::string s_mem_lvl_l2 = "L2";
const std::string s_mem_lvl_l3 = "L3";
const std::string s_mem_lvl_loc_ram = "Local RAM";
const std::string s_mem_lvl_rem_ram1 = "Remote RAM 1 Hop";
const std::string s_mem_lvl_rem_ram2 = "Remote RAM 2 Hops";
const std::string s_mem_lvl_cce1 = "Remote Cache 1 Hops";
const std::string s_mem_lvl_cce2 = "Remote Cache 2 Hops";
const std::string s_mem_lvl_io = "I/O Memory";
const std::string s_mem_lvl_uncached = "Uncached Memory";
const std::string s_mem_lvl_invalid = "Invalid Data Source";

std::string lookup_mem_lvl(uint64_t data_src) {
    uint64_t lvl_bits = data_src >> PERF_MEM_LVL_SHIFT;

    if(lvl_bits & PERF_MEM_LVL_NA)
        return s_mem_lvl_na;
    else if(lvl_bits & PERF_MEM_LVL_L1)
        return s_mem_lvl_l1;
    else if(lvl_bits & PERF_MEM_LVL_LFB)
        return s_mem_lvl_lfb;
    else if(lvl_bits & PERF_MEM_LVL_L2)
        return s_mem_lvl_l2;
    else if(lvl_bits & PERF_MEM_LVL_L3)
        return s_mem_lvl_l3;
    else if(lvl_bits & PERF_MEM_LVL_LOC_RAM)
        return s_mem_lvl_loc_ram;
    else if(lvl_bits & PERF_MEM_LVL_REM_RAM1)
        return s_mem_lvl_rem_ram1;
    else if(lvl_bits & PERF_MEM_LVL_REM_RAM2)
        return s_mem_lvl_rem_ram2;
    else if(lvl_bits & PERF_MEM_LVL_REM_CCE1)
        return s_mem_lvl_cce1;
    else if(lvl_bits & PERF_MEM_LVL_REM_CCE2)
        return s_mem_lvl_cce2;
    else if(lvl_bits & PERF_MEM_LVL_IO)
        return s_mem_lvl_io;
    else if(lvl_bits & PERF_MEM_LVL_UNC)
        return s_mem_lvl_uncached;

    return s_mem_lvl_invalid;
}

