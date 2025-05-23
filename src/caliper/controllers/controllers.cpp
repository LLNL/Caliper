// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "caliper/ConfigManager.h"
#include <cstring>

namespace
{

const char* event_trace_spec = R"json(
{
 "name": "event-trace",
 "description": "Record a trace of region enter/exit events in .cali format",
 "services": [ "async_event", "event", "recorder", "timer", "trace" ],
 "categories": [ "output", "metadata", "event" ],
 "config": { "CALI_CHANNEL_FLUSH_ON_EXIT" : "false" },
 "options":
 [
  {
   "name": "outdir",
   "description": "Output directory",
   "type": "string",
   "config": { "CALI_RECORDER_DIRECTORY": "{}" }
  },{
   "name": "trace.io",
   "description": "Trace I/O events",
   "type": "bool",
   "services": [ "io" ]
  },{
   "name": "trace.mpi",
   "description": "Trace MPI events",
   "type": "bool",
   "services": [ "mpi" ],
   "config": { "CALI_MPI_BLACKLIST": "MPI_Wtime,MPI_Wtick,MPI_Comm_size,MPI_Comm_rank" }
  },{
   "name": "trace.cuda",
   "description": "Trace CUDA API events",
   "type": "bool",
   "services": [ "cupti" ]
  },{
   "name": "trace.openmp",
   "description": "Trace OpenMP events",
   "type": "bool",
   "services": [ "ompt" ]
  },{
   "name": "trace.kokkos",
   "description": "Trace Kokkos kernels",
   "type": "bool",
   "services": [ "kokkostime" ]
  },{
   "name": "time.inclusive",
   "description": "Record inclusive region times",
   "type": "bool",
   "config": { "CALI_TIMER_INCLUSIVE_DURATION": "true" }
  },{
   "name": "sampling",
   "description": "Enable call-path sampling",
   "type": "bool",
   "services": [ "callpath", "pthread", "sampler", "symbollookup" ],
   "config": { "CALI_SAMPLER_FREQUENCY": "200" }
  },{
   "name": "sample.frequency",
   "description": "Sampling frequency when sampling",
   "type": "int",
   "inherit": "sampling",
   "config": { "CALI_SAMPLER_FREQUENCY": "{}" }
  },{
   "name": "papi.counters",
   "description": "List of PAPI counters to read",
   "type": "string",
   "services": [ "papi" ],
   "config": { "CALI_PAPI_COUNTERS": "{}" }
  },{
   "name": "cuda.activities",
   "description": "Trace CUDA activities",
   "type": "bool",
   "services": [ "cuptitrace" ],
   "config": { "CALI_CUPTITRACE_SNAPSHOT_TIMESTAMPS": "true" }
  },{
   "name": "rocm.activities",
   "description": "Trace ROCm activities",
   "type": "bool",
   "services": [ "roctracer" ],
   "config": { "CALI_ROCTRACER_SNAPSHOT_TIMESTAMPS": "true" }
  },{
   "name": "umpire.allocators",
   "description": "Umpire per-allocator allocation statistics",
   "type": "bool",
   "services": [ "umpire" ],
   "config": { "CALI_UMPIRE_PER_ALLOCATOR_STATISTICS": "true" }
  }
 ]
}
)json";

const char* nvprof_spec = R"json(
{
 "name": "nvprof",
 "services": [ "nvtx" ],
 "description": "Forward Caliper regions to NVidia nvprof (deprecated; use nvtx)",
 "config": { "CALI_CHANNEL_FLUSH_ON_EXIT": "false" }
}
)json";

const char* nvtx_spec = R"json(
{
 "name": "nvtx",
 "services": [ "nvtx" ],
 "description": "Forward Caliper regions to NVidia NSight/NVprof",
 "config": { "CALI_CHANNEL_FLUSH_ON_EXIT": "false" }
}
)json";

const char* roctx_spec = R"json(
{
 "name": "roctx",
 "services": [ "roctx" ],
 "description": "Forward Caliper regions to ROCm rocprofiler",
 "config": { "CALI_CHANNEL_FLUSH_ON_EXIT": "false" }
}
)json";

const char* mpireport_spec = R"json(
{
 "name"        : "mpi-report",
 "services"    : [ "aggregate", "event", "mpi", "mpireport", "timer" ],
 "description" : "Print time spent in MPI functions",
 "categories"  : [ "event", "output" ],
 "config"      :
 { "CALI_CHANNEL_FLUSH_ON_EXIT": "false",
   "CALI_EVENT_TRIGGER": "mpi.function",
   "CALI_EVENT_ENABLE_SNAPSHOT_INFO": "false",
   "CALI_TIMER_INCLUSIVE_DURATION": "false",
   "CALI_MPI_BLACKLIST":
     "MPI_Comm_size,MPI_Comm_rank,MPI_Wtime",
   "CALI_MPIREPORT_WRITE_ON_FINALIZE" : "false",
   "CALI_MPIREPORT_CONFIG":
     "let
        sum#time.duration=scale(sum#time.duration.ns,1e-9)
      select
        mpi.function as Function,
        min(count) as \"Count (min)\",
        max(count) as \"Count (max)\",
        min(sum#time.duration) as \"Time (min)\",
        max(sum#time.duration) as \"Time (max)\",
        avg(sum#time.duration) as \"Time (avg)\",
        percent_total(sum#time.duration) as \"Time %\"
      group by
        mpi.function
      format
        table
      order by
        percent_total#sum#time.duration desc
     "
  }
}
)json";

cali::ConfigManager::ConfigInfo event_trace_controller_info { event_trace_spec, nullptr, nullptr };
cali::ConfigManager::ConfigInfo nvprof_controller_info { nvprof_spec, nullptr, nullptr };
cali::ConfigManager::ConfigInfo nvtx_controller_info { nvtx_spec, nullptr, nullptr };
cali::ConfigManager::ConfigInfo roctx_controller_info { roctx_spec, nullptr, nullptr };
cali::ConfigManager::ConfigInfo mpireport_controller_info { mpireport_spec, nullptr, nullptr };

} // namespace

namespace cali
{

extern ConfigManager::ConfigInfo cuda_activity_profile_controller_info;
extern ConfigManager::ConfigInfo cuda_activity_report_controller_info;
extern ConfigManager::ConfigInfo hatchet_region_profile_controller_info;
extern ConfigManager::ConfigInfo hatchet_sample_profile_controller_info;
extern ConfigManager::ConfigInfo loop_report_controller_info;
extern ConfigManager::ConfigInfo openmp_report_controller_info;
extern ConfigManager::ConfigInfo rocm_activity_report_controller_info;
extern ConfigManager::ConfigInfo rocm_activity_profile_controller_info;
extern ConfigManager::ConfigInfo runtime_report_controller_info;
extern ConfigManager::ConfigInfo sample_report_controller_info;
extern ConfigManager::ConfigInfo spot_controller_info;

const ConfigManager::ConfigInfo* builtin_controllers_table[] = { &cuda_activity_profile_controller_info,
                                                                 &cuda_activity_report_controller_info,
                                                                 &::event_trace_controller_info,
                                                                 &::nvprof_controller_info,
                                                                 &::nvtx_controller_info,
                                                                 &::roctx_controller_info,
                                                                 &::mpireport_controller_info,
                                                                 &hatchet_region_profile_controller_info,
                                                                 &hatchet_sample_profile_controller_info,
                                                                 &loop_report_controller_info,
                                                                 &openmp_report_controller_info,
                                                                 &rocm_activity_report_controller_info,
                                                                 &rocm_activity_profile_controller_info,
                                                                 &runtime_report_controller_info,
                                                                 &sample_report_controller_info,
                                                                 &spot_controller_info,
                                                                 nullptr };

const char* builtin_base_option_specs = R"json(
[
{
 "name": "level",
 "type": "string",
 "description": "Minimum region level that triggers snapshots",
 "category": "event",
 "config": { "CALI_EVENT_REGION_LEVEL": "{}" }
},{
 "name": "include_branches",
 "type": "string",
 "description": "Only take snapshots for branches with the given region names.",
 "category": "event",
 "config": { "CALI_EVENT_INCLUDE_BRANCHES": "{}" }
},{
 "name": "include_regions",
 "type": "string",
 "description": "Only take snapshots for the given region names/patterns.",
 "category": "event",
 "config": { "CALI_EVENT_INCLUDE_REGIONS": "{}" }
},{
 "name": "exclude_regions",
 "type": "string",
 "description": "Do not take snapshots for the given region names/patterns.",
 "category": "event",
 "config": { "CALI_EVENT_EXCLUDE_REGIONS": "{}" }
},{
 "name": "region.count",
 "description": "Report number of begin/end region instances",
 "type": "bool",
 "category": "metric",
 "query":
 {
  "local": "let rc.count=first(sum#region.count,region.count) select sum(rc.count) as Calls unit count",
  "cross": "select min(sum#rc.count) as \"Calls/rank (min)\" unit count,avg(sum#rc.count) as \"Calls/rank (avg)\" unit count,max(sum#rc.count) as \"Calls/rank (max)\" unit count,sum(sum#rc.count) as \"Calls (total)\" unit count"
 }
},{
 "name": "region.stats",
 "description": "Detailed region timing statistics (min/max/avg time per visit)",
 "type": "bool",
 "category": "metric",
 "services": [ "timer", "event" ],
 "config":
 {
  "CALI_TIMER_INCLUSIVE_DURATION"   : "true",
  "CALI_EVENT_ENABLE_SNAPSHOT_INFO" : "true"
 },
 "query":
 {
  "local": "select sum(sum#region.count) as Visits unit count,min(min#time.inclusive.duration.ns) as \"Nsec/visit (min)\" unit nsec,ratio(sum#time.inclusive.duration.ns,sum#region.count) as \"Nsec/visit (avg)\" unit nsec,max(max#time.inclusive.duration.ns) as \"Nsec/visit (max)\" unit nsec",
  "cross": "select sum(sum#sum#region.count) as Visits unit count,min(min#min#time.inclusive.duration.ns) as \"Nsec/visit (min)\" unit nsec,ratio(sum#sum#time.inclusive.duration.ns,sum#sum#region.count) as \"Nsec/visit (avg)\" unit nsec,max(max#max#time.inclusive.duration.ns) as \"Nsec/visit (max)\" unit nsec"
 }
},{
 "name": "loop.stats",
 "description": "Loop iteration count and time statistics",
 "type": "bool",
 "category": "metric",
 "services": [ "loop_statistics" ],
 "query":
 {
  "local":
  "let
    ls.min=scale(min#iter.duration.ns,1e-9),ls.avg=scale(avg#iter.duration.ns,1e-9),ls.max=scale(max#iter.duration.ns,1e-9)
   select
    max(max#iter.count) as \"Iterations\" unit count,
    min(ls.min) as \"Time/iter (min)\" unit sec,
    avg(ls.avg) as \"Time/iter (avg)\" unit sec,
    max(ls.max) as \"Time/iter (max)\" unit sec",
  "cross":
  "select
    max(max#max#iter.count) as \"Iterations\" unit count,
    min(min#ls.min) as \"Time/iter (min)\" unit sec,
    avg(avg#ls.avg) as \"Time/iter (avg)\" unit sec,
    max(max#ls.max) as \"Time/iter (max)\" unit sec"
 }
},{
 "name": "async_events",
 "description": "Report timed asynchronous events",
 "type": "bool",
 "category": "metric",
 "services": [ "async_event" ],
 "query":
 {
  "local":
  "let
    as.count=first(count) if async.end,as.min=scale(min#event.duration.ns,1e-9),as.avg=scale(avg#event.duration.ns,1e-9),as.max=scale(max#event.duration.ns,1e-9)
   select
    async.end as \"Event\",sum(as.count) as Count,min(as.min) as \"Event time (min)\",avg(as.avg) as \"Event time (avg)\",max(as.max) as \"Event time (max)\"
   group by
    async.end",
  "cross":
  "select
    async.end as \"Event\",sum(sum#as.count) as Count,min(min#as.min) as \"Event time (min)\",avg(avg#as.avg) as \"Event time (avg)\",max(max#as.max) as \"Event time (max)\"
   group by
    async.end"
 }
},{
 "name": "node.order",
 "description": "Report order in which regions first appeared",
 "type": "bool",
 "category": "metric",
 "query": { "local": "select min(aggregate.slot) as \"Node order\"", "cross": "select min(min#aggregate.slot) as \"Node order\"" }
},{
 "name": "local_query",
 "description": "Augment process-local aggregation query",
 "type": "string",
 "category": "metric",
 "query" : { "local": "{}" }
},{
 "name": "cross_query",
 "description": "Augment cross-process aggregation query",
 "type": "string",
 "category": "metric",
 "query" : { "cross": "{}" }
},{
 "name": "output",
 "description": "Output location ('stdout', 'stderr', or filename)",
 "type": "string",
 "category": "output"
},{
 "name": "adiak.import_categories",
 "services": [ "adiak_import" ],
 "description": "Adiak import categories. Comma-separated list of integers.",
 "type": "string",
 "category": "adiak"
},{
 "name": "max_column_width",
 "type": "int",
 "description": "Maximum column width in the tree display",
 "category": "treeformatter"
},{
 "name": "print.metadata",
 "type": "bool",
 "description": "Print program metadata (Caliper globals and Adiak data)",
 "category": "treeformatter"
},{
 "name": "order_as_visited",
 "type": "bool",
 "description": "Print tree nodes in the original visit order",
 "category": "treeformatter",
 "query":
 {
  "local": "let o_a_v.slot=first(aggregate.slot) aggregate min(o_a_v.slot) order by min#o_a_v.slot",
  "cross": "aggregate min(min#o_a_v.slot) order by min#min#o_a_v.slot"
 }
}
]
)json";

const char* builtin_mpi_option_specs = R"json(
[
{
 "name": "profile.mpi",
 "type": "bool",
 "description": "Profile MPI functions",
 "category": "region",
 "services": [ "mpi" ],
 "config": { "CALI_MPI_BLACKLIST": "MPI_Comm_rank,MPI_Comm_size,MPI_Wtick,MPI_Wtime" }
},
{
 "name": "mpi.include",
 "type": "string",
 "description": "Only instrument these MPI functions.",
 "category": "region",
 "config": { "CALI_MPI_WHITELIST": "{}" }
},
{
 "name": "mpi.exclude",
 "type": "string",
 "description": "Do not instrument these MPI functions.",
 "category": "region",
 "config": { "CALI_MPI_BLACKLIST": "{}" }
},
{
 "name": "mpi.message.size",
 "description": "MPI message size",
 "type": "bool",
 "category": "metric",
 "services": [ "mpi" ],
 "config": { "CALI_MPI_MSG_TRACING": "true", "CALI_MPI_BLACKLIST": "MPI_Wtime,MPI_Comm_rank,MPI_Comm_size" },
 "query":
 {
  "local":
  "let
    mms.min=first(min#mpi.msg.size,mpi.msg.size),
    mms.avg=first(avg#mpi.msg.size,mpi.msg.size),
    mms.max=first(max#mpi.msg.size,mpi.msg.size)
   select
    min(mms.min) as \"Msg size (min)\" unit Byte,
    avg(mms.avg) as \"Msg size (avg)\" unit Byte,
    max(mms.max) as \"Msg size (max)\" unit Byte",
  "cross":
  "select
    min(min#mms.min) as \"Msg size (min)\" unit Byte,
    avg(avg#mms.avg) as \"Msg size (avg)\" unit Byte,
    max(max#mms.avg) as \"Msg size (avg)\" unit Byte"
 }
},
{
 "name": "mpi.message.count",
 "description": "Number of MPI send/recv/collective operations",
 "type": "bool",
 "category": "metric",
 "services": [ "mpi" ],
 "config": { "CALI_MPI_MSG_TRACING": "true", "CALI_MPI_BLACKLIST": "MPI_Wtime,MPI_Comm_rank,MPI_Comm_size" },
 "query":
 {
  "local":
  "let
    mmc.recv=first(sum#mpi.recv.count,mpi.recv.count),
    mmc.send=first(sum#mpi.send.count,mpi.send.count),
    mmc.coll=first(sum#mpi.coll.count,mpi.coll.count)
   select
    sum(mmc.send) as \"Msgs sent\" unit count,
    sum(mmc.recv) as \"Msgs recvd\" unit count,
    sum(mmc.coll) as \"Collectives\" unit count",
  "cross":
  "select
    avg(sum#mmc.send) as \"Msgs sent (avg)\" unit count,
    max(sum#mmc.send) as \"Msgs sent (max)\" unit count,
    avg(sum#mmc.recv) as \"Msgs recvd (avg)\" unit count,
    max(sum#mmc.recv) as \"Msgs recvd (max)\" unit count,
    max(sum#mmc.coll) as \"Collectives (max)\" unit count"
 }
},
{
 "name": "comm.stats",
 "description": "MPI message statistics in marked communication regions",
 "type": "bool",
 "category": "metric",
 "services": [ "async_event", "mpi" ],
 "config": { "CALI_MPI_MSG_PATTERN": "true", "CALI_MPI_BLACKLIST": "MPI_Wtime,MPI_Comm_rank,MPI_Comm_size" },
 "query":
 [
  { "level"    : "local",
    "let"      :
    [
     "cs.irecv_gap.min=scale(min#event.duration.ns,1e-9) if async.end=irecv.req_wait_gap",
     "cs.irecv_gap.max=scale(max#event.duration.ns,1e-9) if async.end=irecv.req_wait_gap",
     "cs.irecv_gap.avg=scale(avg#event.duration.ns,1e-9) if async.end=irecv.req_wait_gap"
    ],
    "select"   :
    [
      "min(min#total.send.count) as \"Sends (min)\"",
      "max(max#total.send.count) as \"Sends (max)\"",
      "sum(sum#total.send.count) as \"Sends (total)\"",
      "min(min#total.recv.count) as \"Recvs (min)\"",
      "max(max#total.recv.count) as \"Recvs (max)\"",
      "sum(sum#total.recv.count) as \"Recvs (total)\"",
      "min(min#total.dest.ranks) as \"Dst ranks (min)\"",
      "max(max#total.dest.ranks) as \"Dst ranks (max)\"",
      "avg(avg#total.dest.ranks) as \"Dst ranks (avg)\"",
      "min(min#total.src.ranks)  as \"Src ranks (min)\"",
      "max(max#total.src.ranks)  as \"Src ranks (max)\"",
      "avg(avg#total.src.ranks)  as \"Src ranks (avg)\"",
      "max(max#total.coll.count) as \"Collectives (max)\"",
      "min(min#mpi.send.size) as \"Bytes sent (min)\"",
      "max(max#mpi.send.size) as \"Bytes sent (max)\"",
      "sum(sum#mpi.send.size) as \"Bytes sent (total)\"",
      "min(min#mpi.recv.size) as \"Bytes recv (min)\"",
      "max(max#mpi.recv.size) as \"Bytes recv (max)\"",
      "sum(sum#mpi.recv.size) as \"Bytes recv (total)\"",
      "min(cs.irecv_gap.min) as \"Irecv gap (min)\"",
      "avg(cs.irecv_gap.avg) as \"Irecv gap (avg)\"",
      "max(cs.irecv_gap.max) as \"Irecv gap (max)\""
    ]
  },
  { "level"    : "cross",
    "select"   :
    [
      "min(min#min#total.send.count) as \"Sends (min)\"",
      "max(max#max#total.send.count) as \"Sends (max)\"",
      "sum(sum#sum#total.send.count) as \"Sends (total)\"",
      "min(min#min#total.recv.count) as \"Recvs (min)\"",
      "max(max#max#total.recv.count) as \"Recvs (max)\"",
      "sum(sum#sum#total.recv.count) as \"Recvs (total)\"",
      "min(min#min#total.dest.ranks) as \"Dst ranks (min)\"",
      "max(max#max#total.dest.ranks) as \"Dst ranks (max)\"",
      "avg(avg#avg#total.dest.ranks) as \"Dst ranks (avg)\"",
      "min(min#min#total.src.ranks)  as \"Src ranks (min)\"",
      "max(max#max#total.src.ranks)  as \"Src ranks (max)\"",
      "avg(avg#avg#total.src.ranks)  as \"Src ranks (avg)\"",
      "max(max#max#total.coll.count) as \"Coll (max)\"",
      "min(min#min#mpi.send.size) as \"Bytes sent (min)\"",
      "max(max#max#mpi.send.size) as \"Bytes sent (max)\"",
      "sum(sum#sum#mpi.send.size) as \"Bytes sent (total)\"",
      "min(min#min#mpi.recv.size) as \"Bytes recv (min)\"",
      "max(max#max#mpi.recv.size) as \"Bytes recv (max)\"",
      "sum(sum#sum#mpi.recv.size) as \"Bytes recv (total)\"",
      "min(min#cs.irecv_gap.min) as \"Irecv gap (min)\"",
      "avg(avg#cs.irecv_gap.avg) as \"Irecv gap (avg)\"",
      "max(max#cs.irecv_gap.max) as \"Irecv gap (max)\""
    ]
  }
 ]
}
]
)json";

const char* builtin_gotcha_option_specs = R"json(
[
{
 "name": "main_thread_only",
 "type": "bool",
 "description": "Only include measurements from the main thread in results.",
 "category": "region",
 "services": [ "pthread" ],
 "query": { "local": "where pthread.is_master=true" }
},{
  "name": "io.bytes.written",
  "description": "Report I/O bytes written",
  "type": "bool",
  "category": "metric",
  "services": [ "io" ],
  "query":
  {
   "local": "let ibw.bytes=first(sum#io.bytes.written,io.bytes.written) select sum(ibw.bytes) as \"Bytes written\" unit Byte",
   "cross": "select avg(sum#ibw.bytes) as \"Avg written\" unit Byte,sum(sum#ibw.bytes) as \"Total written\" unit Byte"
  }
},{
 "name": "io.bytes.read",
 "description": "Report I/O bytes read",
 "type": "bool",
 "category": "metric",
 "services": [ "io" ],
 "query":
 {
  "local": "let ibr.bytes=first(sum#io.bytes.read,io.bytes.read) select sum(ibr.bytes) as \"Bytes read\" unit Byte",
  "cross": "select avg(sum#ibr.bytes) as \"Avg read\" unit Byte,sum(sum#ibr.bytes) as \"Total read\" unit Byte"
 }
},{
 "name": "io.bytes",
 "description": "Report I/O bytes written and read",
 "type": "bool",
 "category": "metric",
 "inherit": [ "io.bytes.read", "io.bytes.written" ]
},{
 "name": "io.read.bandwidth",
 "description": "Report I/O read bandwidth",
 "type": "bool",
 "category": "metric",
 "inherit": [ "io.bytes.read" ],
 "query":
 {
  "local": "select io.region as I/O,ratio(ibr.bytes,time.duration.ns,8e3) as \"Read Mbit/s\" unit Mb/s group by io.region",
  "cross": "select avg(ratio#ibr.bytes/time.duration.ns) as \"Avg read Mbit/s\",max(ratio#ibr.bytes/time.duration.ns) as \"Max read Mbit/s\""
 }
},{
 "name": "io.write.bandwidth",
 "description": "Report I/O write bandwidth",
 "type": "bool",
 "category": "metric",
 "inherit": [ "io.bytes.written" ],
 "query":
 {
  "local": "select io.region as I/O,ratio(ibw.bytes,time.duration.ns,8e3) as \"Write Mbit/s\" unit Mb/s group by io.region",
  "cross": "select avg(ratio#ibw.bytes/time.duration.ns) as \"Avg write Mbit/s\",max(ratio#ibw.bytes/time.duration.ns) as \"Max write Mbit/s\""
 }
},{
 "name": "mem.highwatermark",
 "description": "Report memory high-water mark per region",
 "type": "bool",
 "category": "metric",
 "services": [ "allocstats", "sysalloc" ],
 "config": { "CALI_ALLOCSTATS_RECORD_HIGHWATERMARK": "true" },
 "query":
 {
  "local": "let mhwm.bytes=first(max#alloc.region.highwatermark,alloc.region.highwatermark),mhwm=scale(mhwm.bytes,1e-6) select max(mhwm) as \"Mem HWM MB\" unit MB",
  "cross": "select max(max#mhwm) as \"Mem HWM MB\" unit MB"
 }
},{
 "name": "alloc.stats",
 "description": "Report per-region memory allocation info",
 "type": "bool",
 "category": "metric",
 "services": [ "allocstats", "sysalloc" ],
 "config": { "CALI_ALLOCSTATS_RECORD_HIGHWATERMARK": "true" },
 "query":
 {
  "local":
  "select
    max(max#alloc.region.highwatermark) as \"Mem HWM\",
    max(alloc.tally) as \"Alloc tMax\",
    sum(alloc.count) as \"Alloc count\",
    avg(avg#alloc.size) as \"Avg Bytes/alloc\",
    max(max#alloc.size) as \"Max Bytes/alloc\"",
  "cross":
  "select
    max(max#alloc.region.highwatermark) as \"Mem HWM\",
    max(max#alloc.tally) as \"Alloc tMax\",
    sum(sum#alloc.count) as \"Alloc count\",
    avg(avg#alloc.size) as \"Avg Bytes/alloc\",
    max(max#alloc.size) as \"Max Bytes/alloc\""
 }
},{
 "name": "mem.pages",
 "description": "Memory pages used via /proc/self/statm",
 "type": "bool",
 "category": "metric",
 "services": [ "memstat" ],
 "query":
 {
  "local":
  "let
    mem.vmsize=first(max#memstat.vmsize,memstat.vmsize),
    mem.vmrss=first(max#memstat.vmrss,memstat.vmrss),
    mem.data = first(max#memstat.data,memstat.data)
   select
    max(mem.vmsize) as VmSize unit pages,max(mem.vmrss) as VmRSS unit pages,max(mem.data) as Data unit pages",
  "cross":
  "select
    max(max#mem.vmsize) as VmSize unit pages,max(max#mem.vmrss) as VmRSS unit pages,max(max#mem.data) as Data unit pages"
 }
}
]
)json";

const char* builtin_cuda_option_specs = R"json(
[
{
  "name": "profile.cuda",
  "type": "bool",
  "description": "Profile CUDA API functions",
  "category": "region",
  "services": [ "cupti" ]
},
{
  "name": "cuda.memcpy",
  "description": "Report MB copied between host and device with cudaMemcpy",
  "type": "bool",
  "category": "cuptitrace.metric",
  "query":
  [
    { "level": "local",
      "let":
      [
      "cuda.memcpy.dtoh=scale(cupti.memcpy.bytes,1e-6) if cupti.memcpy.kind=DtoH",
      "cuda.memcpy.htod=scale(cupti.memcpy.bytes,1e-6) if cupti.memcpy.kind=HtoD"
      ],
      "select":
      [
      "sum(cuda.memcpy.htod) as \"Copy CPU->GPU\" unit MB",
      "sum(cuda.memcpy.dtoh) as \"Copy GPU->CPU\" unit MB"
      ]
    },
    { "level": "cross", "select":
      [
        "avg(sum#cuda.memcpy.htod) as \"Copy CPU->GPU (avg)\" unit MB",
        "max(sum#cuda.memcpy.htod) as \"Copy CPU->GPU (max)\" unit MB",
        "avg(sum#cuda.memcpy.dtoh) as \"Copy GPU->CPU (avg)\" unit MB",
        "max(sum#cuda.memcpy.dtoh) as \"Copy GPU->CPU (max)\" unit MB"
      ]
    }
  ]
},
{
 "name": "cuda.gputime",
 "description": "Report GPU time in CUDA activities",
 "type": "bool",
 "category": "metric",
 "services": [ "cuptitrace" ],
 "query":
 { 
  "local": "select inclusive_scale(cupti.activity.duration,1e-9) as \"GPU time (I)\" unit sec",
  "cross": "select avg(iscale#cupti.activity.duration) as \"Avg GPU time/rank\" unit sec,min(iscale#cupti.activity.duration) as \"Min GPU time/rank\" unit sec,max(iscale#cupti.activity.duration) as \"Max GPU time/rank\" unit sec,sum(iscale#cupti.activity.duration) as \"Total GPU time\" unit sec"
 }
}
]
)json";

const char* builtin_rocm_option_specs = R"json(
[
{
 "name": "profile.hip",
 "type": "bool",
 "description": "Profile HIP API functions",
 "category": "region",
 "services": [ "roctracer" ],
 "config": { "CALI_ROCTRACER_TRACE_ACTIVITIES": "false" }
},
{
 "name": "rocm.gputime",
 "description": "Report GPU time in AMD ROCm activities",
 "type": "bool",
 "category": "metric",
 "services": [ "roctracer" ],
 "config": { "CALI_ROCTRACER_TRACE_ACTIVITIES": "true", "CALI_ROCTRACER_RECORD_KERNEL_NAMES": "false" },
 "query":
 { 
  "local": "select inclusive_scale(sum#rocm.activity.duration,1e-9) as \"GPU time (I)\" unit sec",
  "cross": "select avg(iscale#sum#rocm.activity.duration) as \"Avg GPU time/rank\" unit sec,min(iscale#sum#rocm.activity.duration) as \"Min GPU time/rank\" unit sec,max(iscale#sum#rocm.activity.duration) as \"Max GPU time/rank\" unit sec,sum(iscale#sum#rocm.activity.duration) as \"Total GPU time\" unit sec"
 }
}
]
)json";

const char* builtin_openmp_option_specs = R"json(
[
{
 "name"        : "openmp.times",
 "description" : "Report time spent in OpenMP work and barrier regions",
 "type"        : "bool",
 "category"    : "metric",
 "services"    : [ "ompt", "timestamp" ],
 "query"  :
 [
  { "level"   : "local",
    "let"     :
    [
     "t.omp.ns=first(sum#time.duration.ns,time.duration.ns)",
     "t.omp.work=scale(t.omp.ns,1e-9) if omp.work",
     "t.omp.sync=scale(t.omp.ns,1e-9) if omp.sync",
     "t.omp.total=first(t.omp.work,t.omp.sync)"
     ],
     "select"  :
     [ "sum(t.omp.work) as \"Time (work)\"    unit sec",
       "sum(t.omp.sync) as \"Time (barrier)\" unit sec"
     ]
   },
   { "level"   : "cross", "select":
     [ "avg(sum#t.omp.work) as \"Time (work) (avg)\" unit sec",
       "avg(sum#t.omp.sync) as \"Time (barrier) (avg)\" unit sec",
       "sum(sum#t.omp.work) as \"Time (work) (total)\" unit sec",
       "sum(sum#t.omp.sync) as \"Time (barrier) (total)\" unit sec"
     ]
   }
 ]
},
{
 "name"        : "openmp.efficiency",
 "description" : "Compute OpenMP efficiency metrics",
 "type"        : "bool",
 "category"    : "metric",
 "inherit"     : [ "openmp.times" ],
 "query"  :
 [
  { "level"   : "local",
    "select"  :
    [ "inclusive_ratio(t.omp.work,t.omp.total,100.0) as \"Work %\"    unit percent",
      "inclusive_ratio(t.omp.sync,t.omp.total,100.0) as \"Barrier %\" unit percent"
    ]
  },
  { "level"   : "cross", "select":
    [ "min(iratio#t.omp.work/t.omp.total) as \"Work % (min)\" unit percent",
      "avg(iratio#t.omp.work/t.omp.total) as \"Work % (avg)\" unit percent",
      "avg(iratio#t.omp.sync/t.omp.total) as \"Barrier % (avg)\" unit percent",
      "max(iratio#t.omp.sync/t.omp.total) as \"Barrier % (max)\" unit percent"
    ]
  }
 ]
},
{
 "name"        : "openmp.threads",
 "description" : "Show OpenMP threads",
 "type"        : "bool",
 "category"    : "metric",
 "services"    : [ "ompt" ],
 "query"  :
 [
  { "level"   : "local",
    "let"     : [ "n.omp.threads=first(omp.num.threads)" ],
    "group by": "omp.thread.id,omp.thread.type",
    "select"  :
    [ "max(n.omp.threads) as \"#Threads\"",
      "omp.thread.id as \"Thread\""
    ]
  },
  { "level"   : "cross",
    "group by": "omp.thread.id,omp.thread.type",
    "select"  :
    [ "max(max#n.omp.threads) as \"#Threads\"",
      "omp.thread.id as Thread"
    ]
  }
 ]
}
]
)json";

const char* builtin_libdw_option_specs = R"json(
[
{
 "name": "source.module",
 "type": "bool",
 "category": "sampling",
 "description": "Report source module (.so/.exe)",
 "services": [ "symbollookup" ],
 "config": { "CALI_SYMBOLLOOKUP_LOOKUP_MODULE": "true" },
 "query":
 {
  "local": "select module#cali.sampler.pc as Module group by module#cali.sampler.pc",
  "cross": "select module#cali.sampler.pc as Module group by module#cali.sampler.pc"
 }
},
{
 "name": "source.function",
 "type": "bool",
 "category": "sampling",
 "description": "Report source function symbol names",
 "services": [ "symbollookup" ],
 "config": { "CALI_SYMBOLLOOKUP_LOOKUP_FUNCTION": "true" },
 "query":
 {
  "local": "select source.function#cali.sampler.pc as Function group by source.function#cali.sampler.pc",
  "cross": "select source.function#cali.sampler.pc as Function group by source.function#cali.sampler.pc"
 }
},
{
 "name": "source.location",
 "type": "bool",
 "category": "sampling",
 "description": "Report source location (file+line)",
 "services": [ "symbollookup" ],
 "config": { "CALI_SYMBOLLOOKUP_LOOKUP_SOURCELOC": "true" },
 "query":
 {
  "local": "select sourceloc#cali.sampler.pc as Source group by sourceloc#cali.sampler.pc",
  "cross": "select sourceloc#cali.sampler.pc as Source group by sourceloc#cali.sampler.pc"
 }
}
]
)json";

const char* builtin_umpire_option_specs = R"json(
[
{
 "name"        : "umpire.totals",
 "description" : "Report umpire allocation statistics (all allocators combined)",
 "type"        : "bool",
 "category"    : "metric",
 "services"    : [ "umpire" ],
 "config"      : { "CALI_UMPIRE_PER_ALLOCATOR_STATISTICS": "false" },
 "query"  :
 [
  { "level"   : "local",
    "let"     :
    [ "umpt.size.bytes=first(max#umpire.total.size,umpire.total.size)",
      "umpt.count=first(max#umpire.total.count,umpire.total.count)",
      "umpt.hwm.bytes=first(max#umpire.total.hwm,umpire.total.hwm)",
      "umpt.size=scale(umpt.size.bytes,1e-6)",
      "umpt.hwm=scale(umpt.hwm.bytes,1e-6)"
    ],
    "select"  :
    [ "inclusive_max(umpt.size)  as \"Ump MB (Total)\" unit MB",
      "inclusive_max(umpt.count) as \"Ump allocs (Total)\"",
      "inclusive_max(umpt.hwm)   as \"Ump HWM (Total)\""
    ]
  },
  { "level"   : "cross",
    "select"  :
    [ "avg(imax#umpt.size)  as \"Ump MB (avg)\" unit MB",
      "max(imax#umpt.size)  as \"Ump MB (max)\" unit MB",
      "avg(imax#umpt.count) as \"Ump allocs (avg)\"",
      "max(imax#umpt.count) as \"Ump allocs (max)\"",
      "max(imax#umpt.hwm)   as \"Ump HWM (max)\""
    ]
  }
 ]
},
{
 "name"        : "umpire.allocators",
 "description" : "Report umpire allocation statistics per allocator",
 "type"        : "bool",
 "category"    : "metric",
 "services"    : [ "umpire" ],
 "config"      : { "CALI_UMPIRE_PER_ALLOCATOR_STATISTICS": "true" },
 "query"  :
 [
  { "level"   : "local",
    "let"     :
    [ "ump.size.bytes=first(max#umpire.alloc.current.size,umpire.alloc.current.size)",
      "ump.hwm.bytes=first(max#umpire.alloc.highwatermark,umpire.alloc.highwatermark)",
      "ump.count=first(max#umpire.alloc.count,umpire.alloc.count)",
      "ump.size=scale(ump.size.bytes,1e-6)",
      "ump.hwm=scale(ump.hwm.bytes,1e-6)"
     ],
    "select"  :
    [ "umpire.alloc.name as Allocator",
      "inclusive_max(ump.size)  as \"Alloc MB\"  unit MB",
      "inclusive_max(ump.hwm)   as \"Alloc HWM\" unit MB",
      "inclusive_max(ump.count) as \"Num allocs\""
    ],
    "group by": "umpire.alloc.name"
  },
  { "level"   : "cross",
    "select"  :
    [ "umpire.alloc.name   as Allocator",
      "avg(imax#ump.size)  as \"Alloc MB (avg)\"  unit MB",
      "max(imax#ump.size)  as \"Alloc MB (max)\"  unit MB",
      "avg(imax#ump.hwm)   as \"Alloc HWM (avg)\" unit MB",
      "max(imax#ump.hwm)   as \"Alloc HWM (max)\" unit MB",
      "avg(imax#ump.count) as \"Num allocs (avg)\"",
      "max(imax#ump.count) as \"Num allocs (max)\""
    ],
    "group by": "umpire.alloc.name"
  }
 ]
},
{
 "name"        : "umpire.filter",
 "description" : "Names of Umpire allocators to track",
 "type"        : "string",
 "category"    : "metric",
 "config"      : { "CALI_UMPIRE_ALLOCATOR_FILTER": "{}" }
}
]
)json";

const char* builtin_pcp_option_specs = R"json(
[
{
  "name"        : "mem.read.bandwidth",
  "description" : "Record memory read bandwidth using the Performance Co-pilot API",
  "type"        : "bool",
  "category"    : "metric",
  "services"    : [ "pcp.memory" ],
  "query"  :
  [
    { "level"   : "local",
      "let"     : [ "mrb.time=first(pcp.time.duration,sum#pcp.time.duration)" ],
      "select"  : [ "ratio(mem.bytes.read,mrb.time,1e-6) as \"MB/s (r)\" unit MB/s" ]
    },
    { "level"   : "cross", "select":
    [
      "avg(ratio#mem.bytes.read/mrb.time) as \"Avg MemBW (r) (MB/s)\"   unit MB/s",
      "max(ratio#mem.bytes.read/mrb.time) as \"Max MemBW (r) (MB/s)\"   unit MB/s",
      "sum(ratio#mem.bytes.read/mrb.time) as \"Total MemBW (r) (MB/s)\" unit MB/s"
    ]
    }
  ]
},
{
  "name"        : "mem.write.bandwidth",
  "description" : "Record memory write bandwidth using the Performance Co-pilot API",
  "type"        : "bool",
  "category"    : "metric",
  "services"    : [ "pcp.memory" ],
  "query"  :
  [
    { "level"   : "local",
      "let"     : [ "mwb.time=first(pcp.time.duration,sum#pcp.time.duration)" ],
      "select"  : [ "ratio(mem.bytes.written,mwb.time,1e-6) as \"MB/s (w)\" unit MB/s" ]
    },
    { "level"   : "cross", "select":
    [
      "avg(ratio#mem.bytes.written/mwb.time) as \"Avg MemBW (w) (MB/s)\"   unit MB/s",
      "max(ratio#mem.bytes.written/mwb.time) as \"Max MemBW (w) (MB/s)\"   unit MB/s",
      "sum(ratio#mem.bytes.written/mwb.time) as \"Total MemBW (w) (MB/s)\" unit MB/s",
    ]
    }
  ]
},
{
  "name"        : "mem.bandwidth",
  "description" : "Record memory bandwidth using the Performance Co-pilot API",
  "type"        : "bool",
  "category"    : "metric",
  "inherit"     : [ "mem.read.bandwidth", "mem.write.bandwidth" ],
}
]
)json";

const char* builtin_papi_hsw_option_specs = R"json(
[
{
 "name"        : "topdown.toplevel",
 "description" : "Top-down analysis for Intel CPUs (top level)",
 "type"        : "bool",
 "category"    : "metric",
 "services"    : [ "topdown" ],
 "config"      : { "CALI_TOPDOWN_LEVEL": "top" },
 "query"  :
 [
  { "level": "local", "select":
   [
    "any(topdown.retiring) as \"Retiring\"",
    "any(topdown.backend_bound) as \"Backend bound\"",
    "any(topdown.frontend_bound) as \"Frontend bound\"",
    "any(topdown.bad_speculation) as \"Bad speculation\""
   ]
  },
  { "level": "cross", "select":
   [
    "any(any#topdown.retiring) as \"Retiring\"",
    "any(any#topdown.backend_bound) as \"Backend bound\"",
    "any(any#topdown.frontend_bound) as \"Frontend bound\"",
    "any(any#topdown.bad_speculation) as \"Bad speculation\""
   ]
  }
 ]
},
{
 "name"        : "topdown.all",
 "description" : "Top-down analysis for Intel CPUs (all levels)",
 "type"        : "bool",
 "category"    : "metric",
 "services"    : [ "topdown" ],
 "config"      : { "CALI_TOPDOWN_LEVEL": "all" },
 "query"  :
 [
  { "level": "local", "select":
   [
    "any(topdown.retiring) as \"Retiring\"",
    "any(topdown.backend_bound) as \"Backend bound\"",
    "any(topdown.frontend_bound) as \"Frontend bound\"",
    "any(topdown.bad_speculation) as \"Bad speculation\"",
    "any(topdown.branch_mispredict) as \"Branch mispredict\"",
    "any(topdown.machine_clears) as \"Machine clears\"",
    "any(topdown.frontend_latency) as \"Frontend latency\"",
    "any(topdown.frontend_bandwidth) as \"Frontend bandwidth\"",
    "any(topdown.memory_bound) as \"Memory bound\"",
    "any(topdown.core_bound) as \"Core bound\"",
    "any(topdown.ext_mem_bound) as \"External Memory\"",
    "any(topdown.l1_bound) as \"L1 bound\"",
    "any(topdown.l2_bound) as \"L2 bound\"",
    "any(topdown.l3_bound) as \"L3 bound\""
   ]
  },
  { "level": "cross", "select":
   [
    "any(any#topdown.retiring) as \"Retiring\"",
    "any(any#topdown.backend_bound) as \"Backend bound\"",
    "any(any#topdown.frontend_bound) as \"Frontend bound\"",
    "any(any#topdown.bad_speculation) as \"Bad speculation\"",
    "any(any#topdown.branch_mispredict) as \"Branch mispredict\"",
    "any(any#topdown.machine_clears) as \"Machine clears\"",
    "any(any#topdown.frontend_latency) as \"Frontend latency\"",
    "any(any#topdown.frontend_bandwidth) as \"Frontend bandwidth\"",
    "any(any#topdown.memory_bound) as \"Memory bound\"",
    "any(any#topdown.core_bound) as \"Core bound\"",
    "any(any#topdown.ext_mem_bound) as \"External Memory\"",
    "any(any#topdown.l1_bound) as \"L1 bound\"",
    "any(any#topdown.l2_bound) as \"L2 bound\"",
    "any(any#topdown.l3_bound) as \"L3 bound\""
   ]
  }
 ]
}
]
)json";

#ifdef CALIPER_WITH_PAPI_RDPMC
const char* builtin_papi_spr_option_specs = R"json(
[
  {
   "name"        : "topdown.toplevel",
   "description" : "Top-down analysis for Intel CPUs (top level)",
   "type"        : "bool",
   "category"    : "metric",
   "services"    : [ "topdown" ],
   "config"      : { "CALI_TOPDOWN_LEVEL": "top" },
   "query"  :
   [
    { "level": "local", "select":
     [
      "any(topdown.retiring) as \"Retiring\"",
      "any(topdown.backend_bound) as \"Backend bound\"",
      "any(topdown.frontend_bound) as \"Frontend bound\"",
      "any(topdown.bad_speculation) as \"Bad speculation\""
     ]
    },
    { "level": "cross", "select":
     [
      "any(any#topdown.retiring) as \"Retiring\"",
      "any(any#topdown.backend_bound) as \"Backend bound\"",
      "any(any#topdown.frontend_bound) as \"Frontend bound\"",
      "any(any#topdown.bad_speculation) as \"Bad speculation\""
     ]
    }
   ]
  },
  {
   "name"        : "topdown.all",
   "description" : "Top-down analysis for Intel CPUs (all levels)",
   "type"        : "bool",
   "category"    : "metric",
   "services"    : [ "topdown" ],
   "config"      : { "CALI_TOPDOWN_LEVEL": "all" },
   "query"  :
   [
    { "level": "local", "select":
     [
      "any(topdown.retiring) as \"Retiring\"",
      "any(topdown.backend_bound) as \"Backend bound\"",
      "any(topdown.frontend_bound) as \"Frontend bound\"",
      "any(topdown.bad_speculation) as \"Bad speculation\"",
      "any(topdown.branch_mispredict) as \"Branch mispredict\"",
      "any(topdown.machine_clears) as \"Machine clears\"",
      "any(topdown.frontend_latency) as \"Frontend latency\"",
      "any(topdown.frontend_bandwidth) as \"Frontend bandwidth\"",
      "any(topdown.memory_bound) as \"Memory bound\"",
      "any(topdown.core_bound) as \"Core bound\"",
      "any(topdown.light_ops) as \"Light operations\"",
      "any(topdown.heavy_ops) as \"Heavy operations\""
     ]
    },
    { "level": "cross", "select":
     [
      "any(any#topdown.retiring) as \"Retiring\"",
      "any(any#topdown.backend_bound) as \"Backend bound\"",
      "any(any#topdown.frontend_bound) as \"Frontend bound\"",
      "any(any#topdown.bad_speculation) as \"Bad speculation\"",
      "any(any#topdown.branch_mispredict) as \"Branch mispredict\"",
      "any(any#topdown.machine_clears) as \"Machine clears\"",
      "any(any#topdown.frontend_latency) as \"Frontend latency\"",
      "any(any#topdown.frontend_bandwidth) as \"Frontend bandwidth\"",
      "any(any#topdown.memory_bound) as \"Memory bound\"",
      "any(any#topdown.core_bound) as \"Core bound\"",
      "any(any#topdown.light_ops) as \"Light operations\"",
      "any(any#topdown.heavy_ops) as \"Heavy operations\""
     ]
    }
   ]
  },
  {
   "name"        : "topdown-counters.toplevel",
   "description" : "Raw counter values for Intel top-down analysis (top level)",
   "type"        : "bool",
   "category"    : "metric",
   "services"    : [ "papi" ],
   "config"      :
   {
     "CALI_PAPI_COUNTERS":
       "perf::slots,perf::topdown-retiring"
   },
   "query"  :
   [
    { "level": "local", "select":
     [
      "inclusive_sum(sum#papi.slots) as slots",
      "inclusive_sum(sum#papi.perf::topdown-retiring) as topdown_retiring"
     ]
    },
    { "level": "cross", "select":
     [
      "sum(inclusive#sum#papi.slots) as slots",
      "sum(inclusive#sum#papi.perf::topdown-retiring) as topdown_retiring"
     ]
    }
   ]
  },
  {
   "name"        : "topdown-counters.all",
   "description" : "Raw counter values for Intel top-down analysis (all levels)",
   "type"        : "bool",
   "category"    : "metric",
   "services"    : [ "papi" ],
   "config"      :
   {
     "CALI_PAPI_COUNTERS":
       "perf::slots,perf::topdown-retiring"
   },
   "query"  :
   [
    { "level": "local", "select":
     [
      "inclusive_sum(sum#papi.slots) as slots",
      "inclusive_sum(sum#papi.perf::topdown-retiring) as topdown_retiring"
     ]
    },
    { "level": "cross", "select":
     [
      "sum(inclusive#sum#papi.slots) as slots",
      "sum(inclusive#sum#papi.perf::topdown-retiring) as topdown_retiring"
     ]
    }
   ]
  }
]
)json";
#else
const char* builtin_papi_spr_option_specs = R"json(
[
  {
   "name"        : "topdown.toplevel",
   "description" : "Top-down analysis for Intel CPUs (top level)",
   "type"        : "bool",
   "category"    : "metric",
   "services"    : [ "topdown" ],
   "config"      : { "CALI_TOPDOWN_LEVEL": "top" },
   "query"  :
   [
    { "level": "local", "select":
     [
      "any(topdown.retiring) as \"Retiring\"",
      "any(topdown.backend_bound) as \"Backend bound\"",
      "any(topdown.frontend_bound) as \"Frontend bound\"",
      "any(topdown.bad_speculation) as \"Bad speculation\""
     ]
    },
    { "level": "cross", "select":
     [
      "any(any#topdown.retiring) as \"Retiring\"",
      "any(any#topdown.backend_bound) as \"Backend bound\"",
      "any(any#topdown.frontend_bound) as \"Frontend bound\"",
      "any(any#topdown.bad_speculation) as \"Bad speculation\""
     ]
    }
   ]
  },
  {
   "name"        : "topdown.all",
   "description" : "Top-down analysis for Intel CPUs (all levels)",
   "type"        : "bool",
   "category"    : "metric",
   "services"    : [ "topdown" ],
   "config"      : { "CALI_TOPDOWN_LEVEL": "all" },
   "query"  :
   [
    { "level": "local", "select":
     [
      "any(topdown.retiring) as \"Retiring\"",
      "any(topdown.backend_bound) as \"Backend bound\"",
      "any(topdown.frontend_bound) as \"Frontend bound\"",
      "any(topdown.bad_speculation) as \"Bad speculation\"",
      "any(topdown.branch_mispredict) as \"Branch mispredict\"",
      "any(topdown.machine_clears) as \"Machine clears\"",
      "any(topdown.frontend_latency) as \"Frontend latency\"",
      "any(topdown.frontend_bandwidth) as \"Frontend bandwidth\"",
      "any(topdown.memory_bound) as \"Memory bound\"",
      "any(topdown.core_bound) as \"Core bound\"",
      "any(topdown.light_ops) as \"Light operations\"",
      "any(topdown.heavy_ops) as \"Heavy operations\""
     ]
    },
    { "level": "cross", "select":
     [
      "any(any#topdown.retiring) as \"Retiring\"",
      "any(any#topdown.backend_bound) as \"Backend bound\"",
      "any(any#topdown.frontend_bound) as \"Frontend bound\"",
      "any(any#topdown.bad_speculation) as \"Bad speculation\"",
      "any(any#topdown.branch_mispredict) as \"Branch mispredict\"",
      "any(any#topdown.machine_clears) as \"Machine clears\"",
      "any(any#topdown.frontend_latency) as \"Frontend latency\"",
      "any(any#topdown.frontend_bandwidth) as \"Frontend bandwidth\"",
      "any(any#topdown.memory_bound) as \"Memory bound\"",
      "any(any#topdown.core_bound) as \"Core bound\"",
      "any(any#topdown.light_ops) as \"Light operations\"",
      "any(any#topdown.heavy_ops) as \"Heavy operations\""
     ]
    }
   ]
  },
  {
   "name"        : "topdown-counters.toplevel",
   "description" : "Raw counter values for Intel top-down analysis (top level)",
   "type"        : "bool",
   "category"    : "metric",
   "services"    : [ "papi" ],
   "config"      :
   {
     "CALI_PAPI_COUNTERS":
       "perf::slots,perf::topdown-retiring,perf::topdown-bad-spec,perf::topdown-fe-bound,perf::topdown-be-bound,INT_MISC:UOP_DROPPING"
   },
   "query"  :
   [
    { "level": "local", "select":
     [
      "inclusive_sum(sum#papi.perf::slots) as slots",
      "inclusive_sum(sum#papi.perf::topdown-retiring) as topdown_retiring",
      "inclusive_sum(sum#papi.perf::topdown-bad-spec) as topdown_bad_spec",
      "inclusive_sum(sum#papi.perf::topdown-fe-bound) as topdown_fe_bound",
      "inclusive_sum(sum#papi.perf::topdown-be-bound) as topdown_be_bound",
      "inclusive_sum(sum#papi.INT_MISC:UOP_DROPPING) as int_mist:uop_dropping"
     ]
    },
    { "level": "cross", "select":
     [
      "sum(inclusive#sum#papi.perf::slots) as slots",
      "sum(inclusive#sum#papi.perf::topdown-retiring) as topdown_retiring",
      "sum(inclusive#sum#papi.perf::topdown-bad-spec) as topdown_bad_spec",
      "sum(inclusive#sum#papi.perf::topdown-fe-bound) as topdown_fe_bound",
      "sum(inclusive#sum#papi.perf::topdown-be-bound) as topdown_be_bound",
      "sum(inclusive#sum#papi.INT_MISC:UOP_DROPPING) as int_mist:uop_dropping"
     ]
    }
   ]
  },
  {
   "name"        : "topdown-counters.all",
   "description" : "Raw counter values for Intel top-down analysis (all levels)",
   "type"        : "bool",
   "category"    : "metric",
   "services"    : [ "papi" ],
   "config"      :
   {
     "CALI_PAPI_COUNTERS":
       "perf::slots,perf::topdown-retiring,perf::topdown-bad-spec,perf::topdown-fe-bound,perf::topdown-be-bound,INT_MISC:UOP_DROPPING,perf_raw::r8400,perf_raw::r8500,perf_raw::r8600,perf_raw::r8700"
   },
   "query"  :
   [
    { "level": "local", "select":
     [
      "inclusive_sum(sum#papi.perf::slots) as slots",
      "inclusive_sum(sum#papi.perf::topdown-retiring) as topdown_retiring",
      "inclusive_sum(sum#papi.perf::topdown-bad-spec) as topdown_bad_spec",
      "inclusive_sum(sum#papi.perf::topdown-fe-bound) as topdown_fe_bound",
      "inclusive_sum(sum#papi.perf::topdown-be-bound) as topdown_be_bound",
      "inclusive_sum(sum#papi.INT_MISC:UOP_DROPPING) as int_mist:uop_dropping",
      "inclusive_sum(sum#papi.perf_raw::r8400) as topdown_heavy_ops",
      "inclusive_sum(sum#papi.perf_raw::r8500) as topdown_br_mispredict",
      "inclusive_sum(sum#papi.perf_raw::r8600) as topdown_fetch_lat",
      "inclusive_sum(sum#papi.perf_raw::r8700) as topdown_mem_bound"
     ]
    },
    { "level": "cross", "select":
     [
      "sum(inclusive#sum#papi.perf::slots) as slots",
      "sum(inclusive#sum#papi.perf::topdown-retiring) as topdown_retiring",
      "sum(inclusive#sum#papi.perf::topdown-bad-spec) as topdown_bad_spec",
      "sum(inclusive#sum#papi.perf::topdown-fe-bound) as topdown_fe_bound",
      "sum(inclusive#sum#papi.perf::topdown-be-bound) as topdown_be_bound",
      "sum(inclusive#sum#papi.INT_MISC:UOP_DROPPING) as int_mist:uop_dropping",
      "sum(inclusive#sum#papi.perf_raw::r8400) as topdown_heavy_ops",
      "sum(inclusive#sum#papi.perf_raw::r8500) as topdown_br_mispredict",
      "sum(inclusive#sum#papi.perf_raw::r8600) as topdown_fetch_lat",
      "sum(inclusive#sum#papi.perf_raw::r8700) as topdown_mem_bound"
     ]
    }
   ]
  }
]
)json";
#endif

const char* builtin_kokkos_option_specs = R"json(
[
{
 "name": "profile.kokkos",
 "type": "bool",
 "description": "Profile Kokkos functions",
 "category": "region",
 "services": [ "kokkostime" ]
}
]
)json";

} // namespace cali
