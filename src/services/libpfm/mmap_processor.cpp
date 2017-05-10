#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include "PerfEvent.hpp"
#include "mmap_processor.h"

void skip_mmap_buffer(struct perf_event_mmap_page *mmap_buf, size_t sz)
{
    if ((mmap_buf->data_tail + sz) > mmap_buf->data_head)
        sz = mmap_buf->data_head - mmap_buf->data_tail;

    mmap_buf->data_tail += sz;
}

int read_mmap_buffer(struct perf_event_mmap_page *mmap_buf, size_t pgmsk, char *out, size_t sz)
{
	char *data;
	unsigned long tail;
	size_t avail_sz, m, c;

	data = ((char *)mmap_buf)+sysconf(_SC_PAGESIZE);
	tail = mmap_buf->data_tail & pgmsk;
	avail_sz = mmap_buf->data_head - mmap_buf->data_tail;
	if (sz > avail_sz)
		return -1;
	c = pgmsk + 1 -  tail;
	m = c < sz ? c : sz;
	memcpy(out, data+tail, m);
	if ((sz - m) > 0)
		memcpy(out+m, data, sz - m);
	mmap_buf->data_tail += sz;

	return 0;
}

void process_lost_sample(struct perf_event_mmap_page *mmap_buf, size_t pgmsk)
{
    int ret;
	struct { uint64_t id, lost; } lost;

	ret = read_mmap_buffer(mmap_buf, pgmsk, (char*)&lost, sizeof(lost));
}

void process_exit_sample(struct perf_event_mmap_page *mmap_buf, size_t pgmsk)
{
	int ret;
	struct { pid_t pid, ppid, tid, ptid; } grp;

	ret = read_mmap_buffer(mmap_buf, pgmsk, (char*)&grp, sizeof(grp));
}

void process_freq_sample(struct perf_event_mmap_page *mmap_buf, size_t pgmsk)
{
	int ret;
	struct { uint64_t time, id, stream_id; } thr;

	ret = read_mmap_buffer(mmap_buf, pgmsk, (char*)&thr, sizeof(thr));
}

int process_single_sample(struct perf_event_sample *pes, 
                          uint32_t event_type, 
                          //sample_handler_fn_t handler_fn,
                          //void* handler_fn_args,
                          struct perf_event_mmap_page *mmap_buf, 
                          size_t pgmsk)
{
    int ret = 0;

    memset(pes,0, sizeof(struct perf_event_sample));
    
    if(event_type &(PERF_SAMPLE_IP))
    {
        ret |= read_mmap_buffer(mmap_buf, pgmsk, (char*)&pes->ip, sizeof(uint64_t));
    }

    if(event_type &(PERF_SAMPLE_TID))
    {
        ret |= read_mmap_buffer(mmap_buf, pgmsk, (char*)&pes->pid, sizeof(uint32_t));
        ret |= read_mmap_buffer(mmap_buf, pgmsk, (char*)&pes->tid, sizeof(uint32_t));
    }

    if(event_type &(PERF_SAMPLE_TIME))
    {
        ret |= read_mmap_buffer(mmap_buf, pgmsk, (char*)&pes->time, sizeof(uint64_t));
    }

    if(event_type &(PERF_SAMPLE_ADDR))
    {
        ret |= read_mmap_buffer(mmap_buf, pgmsk, (char*)&pes->addr, sizeof(uint64_t));
    }

    if(event_type &(PERF_SAMPLE_ID))
    {
        ret |= read_mmap_buffer(mmap_buf, pgmsk, (char*)&pes->id, sizeof(uint64_t));
    }

    if(event_type &(PERF_SAMPLE_STREAM_ID))
    {
        ret |= read_mmap_buffer(mmap_buf, pgmsk, (char*)&pes->stream_id, sizeof(uint64_t));
    }

    if(event_type &(PERF_SAMPLE_CPU))
    {
        ret |= read_mmap_buffer(mmap_buf, pgmsk, (char*)&pes->cpu, sizeof(uint32_t));
        ret |= read_mmap_buffer(mmap_buf, pgmsk, (char*)&pes->res, sizeof(uint32_t));
    }

    if(event_type &(PERF_SAMPLE_PERIOD))
    {
        ret |= read_mmap_buffer(mmap_buf, pgmsk, (char*)&pes->period, sizeof(uint64_t));
    }

    /*
    if(event_type &(PERF_SAMPLE_CALLCHAIN))
    {
        ret |= read_mmap_buffer(mmap_buf, pgmsk, (char*)&pes->nr, sizeof(uint64_t));
        pes->ips = (uint64_t*)malloc(pes->nr*sizeof(uint64_t));
        ret |= read_mmap_buffer(mmap_buf, pgmsk, (char*)pes->ips,pes->nr*sizeof(uint64_t));
    }
    */

    if(event_type &(PERF_SAMPLE_WEIGHT))
    {
        ret |= read_mmap_buffer(mmap_buf, pgmsk, (char*)&pes->weight, sizeof(uint64_t));
    }

    if(event_type &(PERF_SAMPLE_DATA_SRC))
    {
        ret |= read_mmap_buffer(mmap_buf, pgmsk, (char*)&pes->data_src, sizeof(uint64_t));

        pes->mem_hit = datasource_mem_hit(pes->data_src);
        pes->mem_lvl = datasource_mem_lvl(pes->data_src);
        pes->mem_op = datasource_mem_op(pes->data_src);
        pes->mem_snoop = datasource_mem_snoop(pes->data_src);
        pes->mem_tlb = datasource_mem_tlb(pes->data_src);
    }

    // if(handler_fn)
    // {
    //     handler_fn(pes, handler_fn_args);
    // }

    return ret;
}

int process_sample_buffer(struct perf_event_sample *pes,
                          uint32_t event_type, 
                          //sample_handler_fn_t handler_fn,
                          //void* handler_fn_args,
                          struct perf_event_mmap_page *mmap_buf, 
                          size_t pgmsk)
{
    int ret;
    struct perf_event_header ehdr;

    for(;;) 
    {
        ret = read_mmap_buffer(mmap_buf, pgmsk, (char*)&ehdr, sizeof(ehdr));
        if(ret)
            return 0; // no more samples

        switch(ehdr.type) 
        {
            case PERF_RECORD_SAMPLE:
                process_single_sample(pes, event_type, 
                                      //handler_fn, 
                                      //handler_fn_args,
                                      mmap_buf, 
                                      pgmsk);
                break;
            case PERF_RECORD_EXIT:
                process_exit_sample(mmap_buf, pgmsk);
                break;
            case PERF_RECORD_LOST:
                process_lost_sample(mmap_buf, pgmsk);
                break;
            case PERF_RECORD_THROTTLE:
                process_freq_sample(mmap_buf, pgmsk);
                break;
            case PERF_RECORD_UNTHROTTLE:
                process_freq_sample(mmap_buf, pgmsk);
                break;
            default:
                skip_mmap_buffer(mmap_buf, sizeof(ehdr));
        }
    }
}

const char* datasource_mem_hit(uint64_t datasource)
{
    uint64_t lvl_bits = datasource >> PERF_MEM_LVL_SHIFT;

    if(lvl_bits & PERF_MEM_LVL_NA)
        return "Not Available";
    else if(lvl_bits & PERF_MEM_LVL_HIT)
        return "Hit";
    else if(lvl_bits & PERF_MEM_LVL_MISS)
        return "Miss";

    return "Invalid Data Source";
}

const char* datasource_mem_lvl(uint64_t datasource)
{
    uint64_t lvl_bits = datasource >> PERF_MEM_LVL_SHIFT;

    if(lvl_bits & PERF_MEM_LVL_NA)
        return "Not Available";
    else if(lvl_bits & PERF_MEM_LVL_L1)
        return "L1";
    else if(lvl_bits & PERF_MEM_LVL_LFB)
        return "LFB";
    else if(lvl_bits & PERF_MEM_LVL_L2)
        return "L2";
    else if(lvl_bits & PERF_MEM_LVL_L3)
        return "L3";
    else if(lvl_bits & PERF_MEM_LVL_LOC_RAM)
        return "Local RAM";
    else if(lvl_bits & PERF_MEM_LVL_REM_RAM1)
        return "Remote RAM 1 Hop";
    else if(lvl_bits & PERF_MEM_LVL_REM_RAM2)
        return "Remote RAM 2 Hops";
    else if(lvl_bits & PERF_MEM_LVL_REM_CCE1)
        return "Remote Cache 1 Hops";
    else if(lvl_bits & PERF_MEM_LVL_REM_CCE2)
        return "Remote Cache 2 Hops";
    else if(lvl_bits & PERF_MEM_LVL_IO)
        return "I/O Memory";
    else if(lvl_bits & PERF_MEM_LVL_UNC)
        return "Uncached Memory";

    return "Invalid Data Source";
}

const char* datasource_mem_op(uint64_t datasource)
{
    uint64_t op_bits = datasource >> PERF_MEM_OP_SHIFT;

    if(op_bits & PERF_MEM_OP_NA)
        return "Not Available";
    else if(op_bits & PERF_MEM_OP_LOAD)
        return "Load";
    else if(op_bits & PERF_MEM_OP_STORE)
        return "Store";
    else if(op_bits & PERF_MEM_OP_PFETCH)
        return "Prefetch";
    else if(op_bits & PERF_MEM_OP_EXEC)
        return "Exec";

    return "Invalid Data Source";
}

const char* datasource_mem_snoop(uint64_t datasource)
{
    uint64_t snoop_bits = datasource >> PERF_MEM_SNOOP_SHIFT;

    if(snoop_bits & PERF_MEM_SNOOP_NA)
        return "Not Available";
    else if(snoop_bits & PERF_MEM_SNOOP_NONE)
        return "Snoop None";
    else if(snoop_bits & PERF_MEM_SNOOP_HIT)
        return "Snoop Hit";
    else if(snoop_bits & PERF_MEM_SNOOP_MISS)
        return "Snoop Miss";
    else if(snoop_bits & PERF_MEM_SNOOP_HITM)
        return "Snoop Hit Modified";

    return "Invalid Data Source";
}

const char* datasource_mem_tlb(uint64_t datasource)
{
    uint64_t tlb_bits = datasource >> PERF_MEM_TLB_SHIFT;

    if(tlb_bits & PERF_MEM_TLB_NA)
        return "Not Available";
    else if(tlb_bits & PERF_MEM_TLB_HIT)
        return "TLB Hit";
    else if(tlb_bits & PERF_MEM_TLB_MISS)
        return "TLB Miss";
    else if(tlb_bits & PERF_MEM_TLB_L1)
        return "TLB L1";
    else if(tlb_bits & PERF_MEM_TLB_L2)
        return "TLB L2";
    else if(tlb_bits & PERF_MEM_TLB_WK)
        return "TLB Hardware Walker";
    else if(tlb_bits & PERF_MEM_TLB_OS)
        return "TLB OS Fault Handler";

    return "Invalid Data Source";
}
