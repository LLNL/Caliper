#include <linux/perf_event.h>
#include <sys/mman.h>

void skip_mmap_buffer(struct perf_event_mmap_page *mmap_buf, size_t sz);
int read_mmap_buffer(struct perf_event_mmap_page *mmap_buf, size_t pgmsk, char *out, size_t sz);
void process_lost_sample(struct perf_event_mmap_page *mmap_buf, size_t pgmsk);
void process_exit_sample(struct perf_event_mmap_page *mmap_buf, size_t pgmsk);
void process_freq_sample(struct perf_event_mmap_page *mmap_buf, size_t pgmsk);
int process_single_sample(struct perf_event_sample *pes, 
                          uint32_t event_type, 
                          //sample_handler_fn_t handler_fn,
                          //void* handler_fn_args,
                          struct perf_event_mmap_page *mmap_buf, 
                          size_t pgmsk);
int process_sample_buffer(struct perf_event_sample *pes,
                          uint32_t event_type, 
                          //sample_handler_fn_t handler_fn,
                          //void* handler_fn_args,
                          struct perf_event_mmap_page *mmap_buf, 
                          size_t pgmsk);

const char* datasource_mem_hit(uint64_t datasource);
const char* datasource_mem_lvl(uint64_t datasource);
const char* datasource_mem_op(uint64_t datasource);
const char* datasource_mem_snoop(uint64_t datasource);
const char* datasource_mem_tlb(uint64_t datasource);
