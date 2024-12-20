/*
 * perf_util.h - helper functions for perf_events
 *
 * Copyright (c) 2009 Google, Inc
 * Contributed by Stephane Eranian <eranian@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __PERF_UTIL_H__
#define __PERF_UTIL_H__

#include <sys/types.h>
#include <inttypes.h>
#include <err.h>
#include <perfmon/pfmlib_perf_event.h>

typedef struct {
    struct perf_event_attr hw;
    uint64_t               values[3];
    uint64_t               prev_values[3];
    char*                  name;
    uint64_t               id; /* event id kernel */
    void*                  buf;
    size_t                 pgmsk;
    int                    group_leader;
    int                    fd;
    int                    max_fds;
    int                    idx;  /* opaque libpfm event identifier */
    int                    cpu;  /* cpu to program */
    char*                  fstr; /* fstr from library, must be freed */
} perf_event_desc_t;

typedef struct perf_event_sample {
    //struct perf_event_header header;
    uint64_t sample_id; /* if PERF_SAMPLE_IDENTIFIER */
    uint64_t ip;        /* if PERF_SAMPLE_IP */
    uint64_t pid, tid;  /* if PERF_SAMPLE_TID */
    uint64_t time;      /* if PERF_SAMPLE_TIME */
    uint64_t addr;      /* if PERF_SAMPLE_ADDR */
    uint64_t id;        /* if PERF_SAMPLE_ID */
    uint64_t stream_id; /* if PERF_SAMPLE_STREAM_ID */
    uint64_t cpu, res;  /* if PERF_SAMPLE_CPU */
    uint64_t period;    /* if PERF_SAMPLE_PERIOD */
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
    uint64_t weight;      /* if PERF_SAMPLE_WEIGHT */
    uint64_t data_src;    /* if PERF_SAMPLE_DATA_SRC */
    uint64_t transaction; /* if PERF_SAMPLE_TRANSACTION */
} perf_event_sample_t;

/* handy shortcut */
#define PERF_FORMAT_SCALE (PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING)

extern int  perf_setup_argv_events(const char** argv, perf_event_desc_t** fd, int* num_fds);
extern int  perf_setup_list_events(const char* events, perf_event_desc_t** fd, int* num_fds);
extern int  perf_read_buffer(perf_event_desc_t* hw, void* buf, size_t sz);
extern void perf_free_fds(perf_event_desc_t* fds, int num_fds);
extern void perf_skip_buffer(perf_event_desc_t* hw, size_t sz);

static inline int perf_read_buffer_32(perf_event_desc_t* hw, void* buf)
{
    return perf_read_buffer(hw, buf, sizeof(uint32_t));
}

static inline int perf_read_buffer_64(perf_event_desc_t* hw, void* buf)
{
    return perf_read_buffer(hw, buf, sizeof(uint64_t));
}

/*
 * values[0] = raw count
 * values[1] = TIME_ENABLED
 * values[2] = TIME_RUNNING
 */
static inline uint64_t perf_scale(uint64_t* values)
{
    uint64_t res = 0;

    if (!values[2] && !values[1] && values[0])
        warnx("WARNING: time_running = 0 = time_enabled, raw count not zero\n");

    if (values[2] > values[1])
        warnx("WARNING: time_running > time_enabled\n");

    if (values[2])
        res = (uint64_t) ((double) values[0] * values[1] / values[2]);
    return res;
}

static inline uint64_t perf_scale_delta(uint64_t* values, uint64_t* prev_values)
{
    double   pval[3], val[3];
    uint64_t res = 0;

    if (!values[2] && !values[1] && values[0])
        warnx("WARNING: time_running = 0 = time_enabled, raw count not zero\n");

    if (values[2] > values[1])
        warnx("WARNING: time_running > time_enabled\n");

    if (values[2] - prev_values[2]) {
        /* covnert everything to double to avoid overflows! */
        pval[0] = prev_values[0];
        pval[1] = prev_values[1];
        pval[2] = prev_values[2];
        val[0]  = values[0];
        val[1]  = values[1];
        val[2]  = values[2];
        res     = (uint64_t) (((val[0] - pval[0]) * (val[1] - pval[1]) / (val[2] - pval[2])));
    }
    return res;
}

/*
 * TIME_RUNNING/TIME_ENABLED
 */
static inline double perf_scale_ratio(uint64_t* values)
{
    if (!values[1])
        return 0.0;

    return values[2] * 1.0 / values[1];
}

static inline int perf_fd2event(perf_event_desc_t* fds, int num_events, int fd)
{
    int i;

    for (i = 0; i < num_events; i++)
        if (fds[i].fd == fd)
            return i;
    return -1;
}

/*
 * id = PERF_FORMAT_ID
 */
static inline int perf_id2event(perf_event_desc_t* fds, int num_events, uint64_t id)
{
    int j;
    for (j = 0; j < num_events; j++)
        if (fds[j].id == id)
            return j;
    return -1;
}

static inline int perf_is_group_leader(perf_event_desc_t* fds, int idx)
{
    return fds[idx].group_leader == idx;
}

extern int perf_get_group_nevents(perf_event_desc_t* fds, int num, int leader);
extern int perf_display_sample(perf_event_desc_t* fds, int num_fds, int idx, struct perf_event_header* ehdr, FILE* fp);
extern int perf_read_sample(
    perf_event_desc_t*        fds,
    int                       num_fds,
    int                       idx,
    struct perf_event_header* ehdr,
    perf_event_sample_t*      s,
    FILE*                     ferr
);
extern uint64_t display_lost(perf_event_desc_t* hw, perf_event_desc_t* fds, int num_fds, FILE* fp);
extern void     display_exit(perf_event_desc_t* hw, FILE* fp);
extern void     display_freq(int mode, perf_event_desc_t* hw, FILE* fp);
#endif
