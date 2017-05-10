
#include <linux/perf_event.h>

struct perf_event_container {
    int fd;
    struct perf_event_attr attr;
    struct perf_event_mmap_page *mmap_buf;
};

struct perf_event_sample 
{
    //struct perf_event_header header;
    uint64_t   sample_id;           /* if PERF_SAMPLE_IDENTIFIER */
    uint64_t   ip;                  /* if PERF_SAMPLE_IP */
    uint32_t   pid, tid;            /* if PERF_SAMPLE_TID */
    uint64_t   time;                /* if PERF_SAMPLE_TIME */
    uint64_t   addr;                /* if PERF_SAMPLE_ADDR */
    uint64_t   id;                  /* if PERF_SAMPLE_ID */
    uint64_t   stream_id;           /* if PERF_SAMPLE_STREAM_ID */
    uint32_t   cpu, res;            /* if PERF_SAMPLE_CPU */
    uint64_t   period;              /* if PERF_SAMPLE_PERIOD */
    //struct read_format v;         /* if PERF_SAMPLE_READ */
    //uint64_t   nr;                  /* if PERF_SAMPLE_CALLCHAIN */
    //uint64_t  *ips;                 /* if PERF_SAMPLE_CALLCHAIN */
    //uint32_t   raw_size;            /* if PERF_SAMPLE_RAW */
    //char      *raw_data;            /* if PERF_SAMPLE_RAW */
    //uint64_t   bnr;                 /* if PERF_SAMPLE_BRANCH_STACK */
    //struct perf_branch_entry *lbr; /* if PERF_SAMPLE_BRANCH_STACK */
    //uint64_t   abi;                 /* if PERF_SAMPLE_REGS_USER */
    //uint64_t  *regs;                /* if PERF_SAMPLE_REGS_USER */
    //uint64_t   stack_size;          /* if PERF_SAMPLE_STACK_USER */
    //char      *stack_data;          /* if PERF_SAMPLE_STACK_USER */
    //uint64_t   dyn_size;            /* if PERF_SAMPLE_STACK_USER */
    uint64_t   weight;              /* if PERF_SAMPLE_WEIGHT */
    uint64_t   data_src;            /* if PERF_SAMPLE_DATA_SRC */
    uint64_t   transaction;         /* if PERF_SAMPLE_TRANSACTION */

    size_t data_size;
    size_t num_dims;
    size_t access_index[3];
    const char *data_symbol;

    const char *mem_hit;
    const char *mem_lvl;
    const char *mem_op;
    const char *mem_snoop;
    const char *mem_lock;
    const char *mem_tlb;
};

struct thread_sampler {
    struct perf_event_container *containers;
    struct perf_event_sample pes;
    size_t pgmsk;
};

