#include "perf_postprocessing.h"

#include <linux/perf_event.h>

const std::string s_invalid = "Invalid";
const std::string s_na = "Not Available";

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

const std::string datasource_mem_lvl(uint64_t data_src) {
    uint64_t lvl_bits = data_src >> PERF_MEM_LVL_SHIFT;

    if(lvl_bits & PERF_MEM_LVL_NA)
        return s_na;
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

    return s_invalid;
}

const std::string s_mem_lvl_hit = "Hit";
const std::string s_mem_lvl_miss = "Miss";

const std::string datasource_mem_hit(uint64_t datasource)
{
    uint64_t lvl_bits = datasource >> PERF_MEM_LVL_SHIFT;

    if(lvl_bits & PERF_MEM_LVL_NA)
        return s_na;
    else if(lvl_bits & PERF_MEM_LVL_HIT)
        return s_mem_lvl_hit;
    else if(lvl_bits & PERF_MEM_LVL_MISS)
        return s_mem_lvl_miss;

    return s_invalid;
}

const std::string s_mem_op_na = "Load";
const std::string s_mem_op_load = "Load";
const std::string s_mem_op_store = "Store";
const std::string s_mem_op_pfetch = "Prefetch";
const std::string s_mem_op_exec = "Exec";

const std::string datasource_mem_op(uint64_t datasource)
{
    uint64_t op_bits = datasource >> PERF_MEM_OP_SHIFT;

    if(op_bits & PERF_MEM_OP_NA)
        return s_na;
    else if(op_bits & PERF_MEM_OP_LOAD)
        return s_mem_op_load;
    else if(op_bits & PERF_MEM_OP_STORE)
        return s_mem_op_store;
    else if(op_bits & PERF_MEM_OP_PFETCH)
        return s_mem_op_pfetch;
    else if(op_bits & PERF_MEM_OP_EXEC)
        return s_mem_op_exec;

    return s_invalid;
}

const std::string s_mem_snoop_none = "None";
const std::string s_mem_snoop_hit = "Hit";
const std::string s_mem_snoop_miss = "Miss";
const std::string s_mem_snoop_hitm = "Hit Modified";

const std::string datasource_mem_snoop(uint64_t datasource)
{
    uint64_t snoop_bits = datasource >> PERF_MEM_SNOOP_SHIFT;

    if(snoop_bits & PERF_MEM_SNOOP_NA)
        return s_na;
    else if(snoop_bits & PERF_MEM_SNOOP_NONE)
        return s_mem_snoop_none;
    else if(snoop_bits & PERF_MEM_SNOOP_HIT)
        return s_mem_snoop_hit;
    else if(snoop_bits & PERF_MEM_SNOOP_MISS)
        return s_mem_snoop_miss;
    else if(snoop_bits & PERF_MEM_SNOOP_HITM)
        return s_mem_snoop_hitm;

    return s_invalid;
}

const std::string s_mem_tlb_hit = "Hit";
const std::string s_mem_tlb_miss = "Miss";
const std::string s_mem_tlb_l1 = "L1";
const std::string s_mem_tlb_l2 = "L2";
const std::string s_mem_tlb_wk = "Hardware Walker";
const std::string s_mem_tlb_os = "OS Fault Handler";

const std::string datasource_mem_tlb(uint64_t datasource)
{
    uint64_t tlb_bits = datasource >> PERF_MEM_TLB_SHIFT;

    if(tlb_bits & PERF_MEM_TLB_NA)
        return s_na;
    else if(tlb_bits & PERF_MEM_TLB_HIT)
        return s_mem_tlb_hit;
    else if(tlb_bits & PERF_MEM_TLB_MISS)
        return s_mem_tlb_miss;
    else if(tlb_bits & PERF_MEM_TLB_L1)
        return s_mem_tlb_l1;
    else if(tlb_bits & PERF_MEM_TLB_L2)
        return s_mem_tlb_l2;
    else if(tlb_bits & PERF_MEM_TLB_WK)
        return s_mem_tlb_wk;
    else if(tlb_bits & PERF_MEM_TLB_OS)
        return s_mem_tlb_os;

    return s_invalid;
}
