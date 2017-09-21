#ifndef CALIPER_PERF_POSTPROCESSING_H
#define CALIPER_PERF_POSTPROCESSING_H

#include <sys/types.h>
#include <inttypes.h>

#include <string>

const std::string datasource_mem_lvl(uint64_t data_src);
const std::string datasource_mem_hit(uint64_t datasource);
const std::string datasource_mem_op(uint64_t datasource);
const std::string datasource_mem_snoop(uint64_t datasource);
const std::string datasource_mem_tlb(uint64_t datasource);

#endif //CALIPER_PERF_POSTPROCESSING_H
