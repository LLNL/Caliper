// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "caliper/ConfigManager.h"

namespace
{

const char* event_trace_spec = R"json(
    {
     "name"        : "event-trace",
     "description" : "Record a trace of region enter/exit events in .cali format",
     "services"    : [ "event", "recorder", "timer", "trace" ],
     "categories"  : [ "output", "event" ],
     "config"      :
       { "CALI_CHANNEL_FLUSH_ON_EXIT"   : "false",
         "CALI_TIMER_SNAPSHOT_DURATION" : "true",
         "CALI_TIMER_UNIT"              : "sec"
       },
     "defaults"    : { "event.timestamps": "true" },
     "options":
     [
      { "name"        : "trace.io",
        "description" : "Trace I/O events",
        "type"        : "bool",
        "services"    : [ "io" ]
      },
      { "name"        : "trace.mpi",
        "description" : "Trace MPI events",
        "type"        : "bool",
        "services"    : [ "mpi" ],
        "config"      : { "CALI_MPI_BLACKLIST": "MPI_Wtime,MPI_Wtick,MPI_Comm_size,MPI_Comm_rank" }
      },
      { "name"        : "trace.cuda",
        "description" : "Trace CUDA API events",
        "type"        : "bool",
        "services"    : [ "cupti" ]
      },
      { "name"        : "trace.io",
        "description" : "Trace I/O events",
        "type"        : "bool",
        "services"    : [ "io" ]
      },
      { "name"        : "trace.openmp",
        "description" : "Trace OpenMP events",
        "type"        : "bool",
        "services"    : [ "ompt" ]
      },
      { "name"        : "event.timestamps",
        "description" : "Record event timestamps",
        "type"        : "bool",
        "config"      : { "CALI_TIMER_OFFSET": "true" }
      },
      { "name"        : "sampling",
        "description" : "Enable call-path sampling",
        "type"        : "bool",
        "services"    : [ "callpath", "pthread", "sampler", "symbollookup" ],
        "config"      : { "CALI_SAMPLER_FREQUENCY": "200" }
      },
      { "name"        : "sample.frequency",
        "description" : "Sampling frequency when sampling",
        "type"        : "int",
        "inherit"     : "sampling",
        "config"      : { "CALI_SAMPLER_FREQUENCY": "{}" }
      },
      { "name"        : "papi.counters",
        "description" : "List of PAPI counters to read",
        "type"        : "string",
        "services"    : [ "papi" ],
        "config"      : { "CALI_PAPI_COUNTERS": "{}" }
      },
      { "name"        : "cuda.activities",
        "description" : "Trace CUDA activities",
        "type"        : "bool",
        "services"    : [ "cuptitrace" ],
        "config"      : { "CALI_CUPTITRACE_SNAPSHOT_TIMESTAMPS": "true" }
      },
      { "name"        : "rocm.activities",
        "description" : "Trace ROCm activities",
        "type"        : "bool",
        "services"    : [ "roctracer" ],
        "config"      : { "CALI_ROCTRACER_SNAPSHOT_TIMESTAMPS": "true" }
      },
      { "name"        : "umpire.totals",
        "description" : "Umpire total allocation statistics",
        "type"        : "bool",
        "services"    : [ "umpire" ],
        "config"      : { "CALI_UMPIRE_PER_ALLOCATOR_STATISTICS": "false" }
      },
      { "name"        : "umpire.allocators",
        "description" : "Umpire per-allocator allocation statistics",
        "type"        : "bool",
        "services"    : [ "umpire" ],
        "config"      : { "CALI_UMPIRE_PER_ALLOCATOR_STATISTICS": "true" }
      },
      { "name"        : "umpire.filter",
        "description" : "Names of Umpire allocators to track",
        "type"        : "string",
        "config"      : { "CALI_UMPIRE_ALLOCATOR_FILTER": "{}" }
      }
     ]
    }
)json";

const char* nvprof_spec = R"json(
    {
     "name"        : "nvprof",
     "services"    : [ "nvtx" ],
     "description" : "Forward Caliper regions to NVidia nvprof (deprecated; use nvtx)",
     "config"      : { "CALI_CHANNEL_FLUSH_ON_EXIT": "false" }
    }
)json";

const char* nvtx_spec = R"json(
    {
     "name"        : "nvtx",
     "services"    : [ "nvtx" ],
     "description" : "Forward Caliper regions to NVidia NSight/NVprof",
     "config"      : { "CALI_CHANNEL_FLUSH_ON_EXIT": "false" }
    }
)json";

const char* roctx_spec = R"json(
    {
     "name"        : "roctx",
     "services"    : [ "roctx" ],
     "description" : "Forward Caliper regions to ROCm rocprofiler",
     "config"      : { "CALI_CHANNEL_FLUSH_ON_EXIT": "false" }
    }
)json";

const char* mpireport_spec = R"json(
    {
     "name"        : "mpi-report",
     "services"    : [ "aggregate", "event", "mpi", "mpireport", "timer" ],
     "description" : "Print time spent in MPI functions",
     "categories"  : [ "event" ],
     "config"      :
      { "CALI_CHANNEL_FLUSH_ON_EXIT"       : "false",
        "CALI_AGGREGATE_KEY"               : "mpi.function",
        "CALI_EVENT_TRIGGER"               : "mpi.function",
        "CALI_EVENT_ENABLE_SNAPSHOT_INFO"  : "false",
        "CALI_TIMER_SNAPSHOT_DURATION"     : "true",
        "CALI_TIMER_INCLUSIVE_DURATION"    : "false",
        "CALI_TIMER_UNIT"                  : "sec",
        "CALI_MPI_BLACKLIST"    :
          "MPI_Comm_size,MPI_Comm_rank,MPI_Wtime",
        "CALI_MPIREPORT_WRITE_ON_FINALIZE" : "false",
        "CALI_MPIREPORT_CONFIG" :
          "select
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
cali::ConfigManager::ConfigInfo nvprof_controller_info      { nvprof_spec,      nullptr, nullptr };
cali::ConfigManager::ConfigInfo nvtx_controller_info        { nvtx_spec,        nullptr, nullptr };
cali::ConfigManager::ConfigInfo roctx_controller_info       { roctx_spec,       nullptr, nullptr };
cali::ConfigManager::ConfigInfo mpireport_controller_info   { mpireport_spec,   nullptr, nullptr };

}

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

const ConfigManager::ConfigInfo* builtin_controllers_table[] = {
    &cuda_activity_profile_controller_info,
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
    nullptr
};

const char* builtin_option_specs = R"json(
    [
    {
     "name"        : "profile.mpi",
     "type"        : "bool",
     "description" : "Profile MPI functions",
     "category"    : "region",
     "services"    : [ "mpi" ],
     "config": { "CALI_MPI_BLACKLIST": "MPI_Comm_rank,MPI_Comm_size,MPI_Wtick,MPI_Wtime" }
    },
    {
     "name"        : "profile.cuda",
     "type"        : "bool",
     "description" : "Profile CUDA API functions",
     "category"    : "region",
     "services"    : [ "cupti" ]
    },
    {
     "name"        : "profile.hip",
     "type"        : "bool",
     "description" : "Profile HIP API functions",
     "category"    : "region",
     "services"    : [ "roctracer" ],
     "config"      : { "CALI_ROCTRACER_TRACE_ACTIVITIES": "false" }
    },
    {
     "name"        : "profile.kokkos",
     "type"        : "bool",
     "description" : "Profile Kokkos functions",
     "category"    : "region",
     "services"    : [ "kokkostime" ]
    },
    {
     "name"        : "main_thread_only",
     "type"        : "bool",
     "description" : "Only include measurements from the main thread in results.",
     "category"    : "region",
     "services"    : [ "pthread" ],
     "query"  :
     [
      { "level"    : "local",
        "where"    : "pthread.is_master=true"
      }
     ]
    },
    {
     "name"        : "include_regions",
     "type"        : "string",
     "description" : "Only take snapshots for the given region names/patterns.",
     "category"    : "event",
     "config"      : { "CALI_EVENT_INCLUDE_REGIONS": "{}" }
    },
    {
     "name"        : "exclude_regions",
     "type"        : "string",
     "description" : "Do not take snapshots for the given region names/patterns.",
     "category"    : "event",
     "config"      : { "CALI_EVENT_EXCLUDE_REGIONS": "{}" }
    },
    {
     "name"        : "mpi.include",
     "type"        : "string",
     "description" : "Only instrument these MPI functions.",
     "category"    : "region",
     "config"      : { "CALI_MPI_WHITELIST": "{}" }
    },
    {
     "name"        : "mpi.exclude",
     "type"        : "string",
     "description" : "Do not instrument these MPI functions.",
     "category"    : "region",
     "config"      : { "CALI_MPI_BLACKLIST": "{}" }
    },
    {
     "name"        : "region.count",
     "description" : "Report number of begin/end region instances",
     "type"        : "bool",
     "category"    : "metric",
     "query"  :
     [
       { "level"   : "local",
         "let"     : [ "rc.count=first(sum#region.count,region.count)" ],
         "select"  : [ { "expr": "sum(rc.count)", "as": "Calls", "unit": "count" } ]
       },
       { "level"   : "cross", "select":
         [ { "expr": "min(sum#rc.count)", "as": "Calls (min)",   "unit": "count" },
           { "expr": "max(sum#rc.count)", "as": "Calls (max)",   "unit": "count" },
           { "expr": "avg(sum#rc.count)", "as": "Calls (avg)",   "unit": "count" },
           { "expr": "sum(sum#rc.count)", "as": "Calls (total)", "unit": "count" }
         ]
       }
     ]
    },
    {
     "name"        : "source.module",
     "type"        : "bool",
     "category"    : "sampling",
     "description" : "Report source module (.so/.exe)",
     "services"    : [ "symbollookup" ],
     "config"      : { "CALI_SYMBOLLOOKUP_LOOKUP_MODULE": "true" },
     "query":
     [
      { "level": "local", "group by": "module#cali.sampler.pc",
        "select": [ { "expr": "module#cali.sampler.pc", "as": "Module" } ]
      },
      { "level": "cross", "group by": "module#cali.sampler.pc",
        "select": [ { "expr": "module#cali.sampler.pc", "as": "Module" } ]
      }
     ]
    },
    {
     "name"        : "source.function",
     "type"        : "bool",
     "category"    : "sampling",
     "description" : "Report source function symbol names",
     "services"    : [ "symbollookup" ],
     "config"      : { "CALI_SYMBOLLOOKUP_LOOKUP_FUNCTION": "true" },
     "query":
     [
      { "level": "local", "group by": "source.function#cali.sampler.pc",
        "select": [ { "expr": "source.function#cali.sampler.pc", "as": "Function" } ]
      },
      { "level": "cross", "group by": "source.function#cali.sampler.pc",
        "select": [ { "expr": "source.function#cali.sampler.pc", "as": "Function" } ]
      }
     ]
    },
    {
     "name"        : "source.location",
     "type"        : "bool",
     "category"    : "sampling",
     "description" : "Report source location (file+line)",
     "services"    : [ "symbollookup" ],
     "config"      : { "CALI_SYMBOLLOOKUP_LOOKUP_SOURCELOC": "true" },
     "query":
     [
      { "level": "local", "group by": "sourceloc#cali.sampler.pc",
        "select": [ { "expr": "sourceloc#cali.sampler.pc", "as": "Source" } ]
      },
      { "level": "cross", "group by": "sourceloc#cali.sampler.pc",
        "select": [ { "expr": "sourceloc#cali.sampler.pc", "as": "Source" } ]
      }
     ]
    },
    {
     "name"        : "cuda.memcpy",
     "description" : "Report MB copied between host and device with cudaMemcpy",
     "type"        : "bool",
     "category"    : "cuptitrace.metric",
     "query"  :
     [
       { "level"   : "local",
         "let"     :
         [
          "cuda.memcpy.dtoh=scale(cupti.memcpy.bytes,1e-6) if cupti.memcpy.kind=DtoH",
          "cuda.memcpy.htod=scale(cupti.memcpy.bytes,1e-6) if cupti.memcpy.kind=HtoD"
         ],
         "select"  :
         [ { "expr": "sum(cuda.memcpy.htod)", "as": "Copy CPU->GPU", "unit": "MB" },
           { "expr": "sum(cuda.memcpy.dtoh)", "as": "Copy GPU->CPU", "unit": "MB" }
         ]
       },
       { "level"   : "cross", "select":
         [ { "expr": "avg(sum#cuda.memcpy.htod)", "as": "Copy CPU->GPU (avg)", "unit": "MB" },
           { "expr": "max(sum#cuda.memcpy.htod)", "as": "Copy CPU->GPU (max)", "unit": "MB" },
           { "expr": "avg(sum#cuda.memcpy.dtoh)", "as": "Copy GPU->CPU (avg)", "unit": "MB" },
           { "expr": "max(sum#cuda.memcpy.dtoh)", "as": "Copy GPU->CPU (max)", "unit": "MB" }
         ]
       }
     ]
    },
    {
     "name"        : "cuda.gputime",
     "description" : "Report GPU time in CUDA activities",
     "type"        : "bool",
     "category"    : "metric",
     "services"    : [ "cuptitrace" ],
     "query"  :
     [
       { "level"   : "local",
         "select"  :
         [ { "expr": "inclusive_scale(cupti.activity.duration,1e-9)", "as": "GPU Time (I)", "unit": "sec" },
         ]
       },
       { "level"   : "cross", "select":
         [ { "expr": "avg(iscale#cupti.activity.duration)", "as": "Avg GPU Time/rank", "unit": "sec" },
           { "expr": "min(iscale#cupti.activity.duration)", "as": "Min GPU Time/rank", "unit": "sec" },
           { "expr": "max(iscale#cupti.activity.duration)", "as": "Max GPU Time/rank", "unit": "sec" },
           { "expr": "sum(iscale#cupti.activity.duration)", "as": "Total GPU Time", "unit": "sec" }
         ]
       }
     ]
    },
    {
     "name"        : "rocm.gputime",
     "description" : "Report GPU time in AMD ROCm activities",
     "type"        : "bool",
     "category"    : "metric",
     "services"    : [ "roctracer" ],
     "config"      : { "CALI_ROCTRACER_TRACE_ACTIVITIES": "true", "CALI_ROCTRACER_RECORD_KERNEL_NAMES": "false" },
     "query"  :
     [
       { "level"   : "local",
         "select"  :
         [ { "expr": "inclusive_scale(sum#rocm.activity.duration,1e-9)", "as": "GPU Time (I)", "unit": "sec" },
         ]
       },
       { "level"   : "cross", "select":
         [ { "expr": "avg(iscale#rocm.activity.duration)", "as": "Avg GPU Time/rank", "unit": "sec" },
           { "expr": "min(iscale#rocm.activity.duration)", "as": "Min GPU Time/rank", "unit": "sec" },
           { "expr": "max(iscale#rocm.activity.duration)", "as": "Max GPU Time/rank", "unit": "sec" },
           { "expr": "sum(iscale#rocm.activity.duration)", "as": "Total GPU Time", "unit": "sec" }
         ]
       }
     ]
    },
    {
     "name"       : "mpi.message.size",
     "description": "MPI message size",
     "type"       : "bool",
     "category"   : "metric",
     "services"   : [ "mpi" ],
     "config"     : { "CALI_MPI_MSG_TRACING": "true", "CALI_MPI_BLACKLIST": "MPI_Wtime,MPI_Comm_rank,MPI_Comm_size" },
     "query"      :
     [
      { "level"   : "local",
        "let"     : [
          "mpimsg.min=first(min#mpi.msg.size,mpi.msg.size)",
          "mpimsg.avg=first(avg#mpi.msg.size,mpi.msg.size)",
          "mpimsg.max=first(max#mpi.msg.size,mpi.msg.size)"
        ],
        "select"  : [
          { "expr": "min(mpimsg.min)", "as": "Msg size (min)", "unit": "Byte" },
          { "expr": "avg(mpimsg.avg)", "as": "Msg size (avg)", "unit": "Byte" },
          { "expr": "max(mpimsg.max)", "as": "Msg size (max)", "unit": "Byte" }
        ]
      },
      { "level"   : "cross",
        "select"  : [
          { "expr": "min(min#mpimsg.min)", "as": "Msg size (min)", "unit": "Byte" },
          { "expr": "avg(avg#mpimsg.avg)", "as": "Msg size (avg)", "unit": "Byte" },
          { "expr": "max(max#mpimsg.max)", "as": "Msg size (max)", "unit": "Byte" }
        ]
      }
     ]
    },
    {
     "name"       : "mpi.message.count",
     "description": "Number of MPI send/recv/collective operations",
     "type"       : "bool",
     "category"   : "metric",
     "services"   : [ "mpi" ],
     "config"     : { "CALI_MPI_MSG_TRACING": "true", "CALI_MPI_BLACKLIST": "MPI_Wtime,MPI_Comm_rank,MPI_Comm_size" },
     "query"      :
     [
      { "level"   : "local",
        "let"     : [
          "mpicount.recv=first(sum#mpi.recv.count,mpi.recv.count)",
          "mpicount.send=first(sum#mpi.send.count,mpi.send.count)",
          "mpicount.coll=first(sum#mpi.coll.count,mpi.coll.count)"
        ],
        "select"  : [
          { "expr": "sum(mpicount.send)", "as": "Msgs sent", "unit": "Count" },
          { "expr": "sum(mpicount.recv)", "as": "Msgs recvd", "unit": "Count" },
          { "expr": "sum(mpicount.coll)", "as": "Collectives", "unit": "Count" }
        ]
      },
      { "level"   : "cross",
        "select"  : [
          { "expr": "avg(sum#mpicount.send)", "as": "Msgs sent (avg)", "unit": "Count" },
          { "expr": "max(sum#mpicount.send)", "as": "Msgs sent (max)", "unit": "Count" },
          { "expr": "avg(sum#mpicount.recv)", "as": "Msgs recvd (avg)", "unit": "Count" },
          { "expr": "max(sum#mpicount.recv)", "as": "Msgs recvd (max)", "unit": "Count" },
          { "expr": "max(sum#mpicount.coll)", "as": "Collectives (max)", "unit": "Count" }
        ]
      }
     ]
    },
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
          "t.omp.work=first(sum#time.duration,time.duration) if omp.work",
          "t.omp.sync=first(sum#time.duration,time.duration) if omp.sync",
          "t.omp.total=first(t.omp.work,t.omp.sync)"
         ],
         "select"  :
         [ { "expr": "sum(t.omp.work)", "as": "Time (work)",    "unit": "sec" },
           { "expr": "sum(t.omp.sync)", "as": "Time (barrier)", "unit": "sec" }
         ]
       },
       { "level"   : "cross", "select":
         [ { "expr": "avg(sum#t.omp.work)", "as": "Time (work) (avg)", "unit": "sec" },
           { "expr": "avg(sum#t.omp.sync)", "as": "Time (barrier) (avg)", "unit": "sec" },
           { "expr": "sum(sum#t.omp.work)", "as": "Time (work) (total)", "unit": "sec" },
           { "expr": "sum(sum#t.omp.sync)", "as": "Time (barrier) (total)", "unit": "sec" }
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
         [ { "expr": "inclusive_ratio(t.omp.work,t.omp.total,100.0)", "as": "Work %",    "unit": "percent" },
           { "expr": "inclusive_ratio(t.omp.sync,t.omp.total,100.0)", "as": "Barrier %", "unit": "percent" }
         ]
       },
       { "level"   : "cross", "select":
         [ { "expr": "min(iratio#t.omp.work/t.omp.total)", "as": "Work % (min)", "unit": "percent" },
           { "expr": "avg(iratio#t.omp.work/t.omp.total)", "as": "Work % (avg)", "unit": "percent" },
           { "expr": "avg(iratio#t.omp.sync/t.omp.total)", "as": "Barrier % (avg)", "unit": "percent" },
           { "expr": "max(iratio#t.omp.sync/t.omp.total)", "as": "Barrier % (max)", "unit": "percent" }
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
         [ { "expr": "max(n.omp.threads)", "as": "#Threads" },
           { "expr": "omp.thread.id", "as": "Thread" }
         ]
       },
       { "level"   : "cross",
         "group by": "omp.thread.id,omp.thread.type",
         "select"  :
         [ { "expr": "max(max#n.omp.threads)", "as": "#Threads" },
           { "expr": "omp.thread.id",   "as": "Thread" }
         ]
       }
     ]
    },
    {
     "name"        : "io.bytes.written",
     "description" : "Report I/O bytes written",
     "type"        : "bool",
     "category"    : "metric",
     "services"    : [ "io" ],
     "query"  :
     [
       { "level"   : "local",
         "let"     : [ "ibw.bytes.written=first(sum#io.bytes.written,io.bytes.written)" ],
         "select"  : [ { "expr": "sum(ibw.bytes.written)", "as": "Bytes written", "unit": "Byte" } ]
       },
       { "level"   : "cross", "select":
         [ { "expr": "avg(sum#ibw.bytes.written)", "as": "Avg written",   "unit": "Byte" },
           { "expr": "sum(sum#ibw.bytes.written)", "as": "Total written", "unit": "Byte" }
         ]
       }
     ]
    },
    {
     "name"        : "io.bytes.read",
     "description" : "Report I/O bytes read",
     "type"        : "bool",
     "category"    : "metric",
     "services"    : [ "io" ],
     "query"  :
     [
       { "level"   : "local",
         "let"     : [ "ibr.bytes.read=first(sum#io.bytes.read,io.bytes.read)" ],
         "select"  : [ { "expr": "sum(ibr.bytes.read)", "as": "Bytes read", "unit": "Byte" } ]
       },
       { "level"   : "cross", "select":
         [ { "expr": "avg(sum#ibr.bytes.read)", "as": "Avg read",   "unit": "Byte" },
           { "expr": "sum(sum#ibr.bytes.read)", "as": "Total read", "unit": "Byte" }
         ]
       }
     ]
    },
    {
     "name"        : "io.bytes",
     "description" : "Report I/O bytes written and read",
     "type"        : "bool",
     "category"    : "metric",
     "inherit"     : [ "io.bytes.read", "io.bytes.written" ]
    },
    {
     "name"        : "io.read.bandwidth",
     "description" : "Report I/O read bandwidth",
     "type"        : "bool",
     "category"    : "metric",
     "services"    : [ "io" ],
     "query"  :
     [
      { "level"    : "local",
        "group by" : "io.region",
        "let"      :
         [
           "irb.bytes.read=first(sum#io.bytes.read,io.bytes.read)",
           "irb.time=first(sum#time.duration,time.duration)"
         ],
        "select"   :
         [
          { "expr": "io.region", "as": "I/O" },
          { "expr": "ratio(irb.bytes.read,irb.time,8e-6)", "as": "Read Mbit/s", "unit": "Mb/s" }
         ]
      },
      { "level": "cross", "select":
       [
        { "expr": "avg(ratio#irb.bytes_read/irb.time)", "as": "Avg read Mbit/s", "unit": "Mb/s" },
        { "expr": "max(ratio#irb.bytes_read/irb.time)", "as": "Max read Mbit/s", "unit": "Mb/s" }
       ]
      }
     ]
    },
    {
     "name"        : "io.write.bandwidth",
     "description" : "Report I/O write bandwidth",
     "type"        : "bool",
     "category"    : "metric",
     "services"    : [ "io" ],
     "query"  :
     [
      { "level"    : "local",
        "group by" : "io.region",
        "let"      :
         [
           "iwb.bytes.written=first(sum#io.bytes.written,io.bytes.written)",
           "iwb.time=first(sum#time.duration,time.duration)"
         ],
        "select"   :
         [
          { "expr": "io.region", "as": "I/O" },
          { "expr": "ratio(iwb.bytes.written,iwb.time,8e-6)", "as": "Write Mbit/s", "unit": "Mb/s" }
         ]
      },
      { "level": "cross", "select":
       [
        { "expr": "avg(ratio#iwb.bytes.written/iwb.time)", "as": "Avg write Mbit/s", "unit": "Mb/s" },
        { "expr": "max(ratio#iwb.bytes.written/iwb.time)", "as": "Max write Mbit/s", "unit": "Mb/s" }
       ]
      }
     ]
    },
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
           [ { "expr": "inclusive_max(umpt.size)", "as": "Ump MB (Total)", "unit": "MB" },
             { "expr": "inclusive_max(umpt.count)", "as": "Ump allocs (Total)" },
             { "expr": "inclusive_max(umpt.hwm)", "as": "Ump HWM (Total)" }
           ]
       },
       { "level"   : "cross",
         "select"  :
           [ { "expr": "avg(imax#umpt.size)", "as": "Ump MB (avg)", "unit": "MB" },
             { "expr": "max(imax#umpt.size)", "as": "Ump MB (max)", "unit": "MB" },
             { "expr": "avg(imax#umpt.count)", "as": "Ump allocs (avg)" },
             { "expr": "max(imax#umpt.count)", "as": "Ump allocs (max)" },
             { "expr": "max(imax#umpt.hwm)", "as": "Ump HWM (max)" }
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
           [ { "expr": "umpire.alloc.name", "as": "Allocator" },
             { "expr": "inclusive_max(ump.size)", "as": "Alloc MB", "unit": "MB" },
             { "expr": "inclusive_max(ump.hwm)", "as": "Alloc HWM", "unit": "MB" },
             { "expr": "inclusive_max(ump.count)", "as": "Num allocs" }
           ],
         "group by": [ "umpire.alloc.name" ]
       },
       { "level"   : "cross",
         "select"  :
           [ { "expr": "umpire.alloc.name", "as": "Allocator" },
             { "expr": "avg(imax#ump.size)", "as": "Alloc MB (avg)", "unit": "MB" },
             { "expr": "max(imax#ump.size)", "as": "Alloc MB (max)", "unit": "MB" },
             { "expr": "avg(imax#ump.hwm)", "as": "Alloc HWM (avg)", "unit": "MB" },
             { "expr": "max(imax#ump.hwm)", "as": "Alloc HWM (max)", "unit": "MB" },
             { "expr": "avg(imax#ump.count)", "as": "Num allocs (avg)" },
             { "expr": "max(imax#ump.count)", "as": "Num allocs (max)" }
           ],
         "group by": [ "umpire.alloc.name" ]
       }
     ]
    },
    {
     "name"        : "umpire.filter",
     "description" : "Names of Umpire allocators to track",
     "type"        : "string",
     "category"    : "metric",
     "config"      : { "CALI_UMPIRE_ALLOCATOR_FILTER": "{}" }
    },
    {
     "name"        : "mem.highwatermark",
     "description" : "Report memory high-water mark",
     "type"        : "bool",
     "category"    : "metric",
     "services"    : [ "alloc", "sysalloc" ],
     "config"      : { "CALI_ALLOC_TRACK_ALLOCATIONS": "false", "CALI_ALLOC_RECORD_HIGHWATERMARK": "true" },
     "query"  :
     [
       { "level"   : "local",
         "let"     :
           [ "mem.highwatermark.bytes = first(max#alloc.region.highwatermark,alloc.region.highwatermark)",
             "mem.highwatermark = scale(mem.highwatermark.bytes,1e-6)"
           ],
         "select"  : [ { "expr": "max(mem.highwatermark)", "as": "Allocated MB", "unit": "MB" } ]
       },
       { "level"   : "cross",
         "select"  : [ { "expr": "max(max#mem.highwatermark)", "as": "Allocated MB", "unit": "MB" } ]
       }
     ]
    },
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
         "select"  : [ { "expr": "ratio(mem.bytes.read,mrb.time,1e-6)", "as": "MB/s (r)", "unit": "MB/s" } ]
       },
       { "level"   : "cross", "select":
        [
         { "expr"  : "avg(ratio#mem.bytes.read/mrb.time)", "as": "Avg MemBW (r) (MB/s)",   "unit": "MB/s" },
         { "expr"  : "max(ratio#mem.bytes.read/mrb.time)", "as": "Max MemBW (r) (MB/s) ",  "unit": "MB/s" },
         { "expr"  : "sum(ratio#mem.bytes.read/mrb.time)", "as": "Total MemBW (r) (MB/s)", "unit": "MB/s" }
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
         "let"     : [ "mwb.time=first(pcp.time.duration,sum#pcp.time.duration) " ],
         "select"  : [ { "expr": "ratio(mem.bytes.written,mwb.time,1e-6)", "as": "MB/s (w)", "unit": "MB/s" } ]
       },
       { "level"   : "cross", "select":
        [
         { "expr"  : "avg(ratio#mem.bytes.written/mwb.time)", "as": "Avg MemBW (w) (MB/s)",   "unit": "MB/s" },
         { "expr"  : "max(ratio#mem.bytes.written/mwb.time)", "as": "Max MemBW (w) (MB/s)",   "unit": "MB/s" },
         { "expr"  : "sum(ratio#mem.bytes.written/mwb.time)", "as": "Total MemBW (w) (MB/s)", "unit": "MB/s" },
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
    },
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
        { "expr": "any(topdown.retiring)", "as": "Retiring" },
        { "expr": "any(topdown.backend_bound)", "as": "Backend bound" },
        { "expr": "any(topdown.frontend_bound)", "as": "Frontend bound" },
        { "expr": "any(topdown.bad_speculation)", "as": "Bad speculation" }
       ]
      },
      { "level": "cross", "select":
       [
        { "expr": "any(any#topdown.retiring)", "as": "Retiring" },
        { "expr": "any(any#topdown.backend_bound)", "as": "Backend bound" },
        { "expr": "any(any#topdown.frontend_bound)", "as": "Frontend bound" },
        { "expr": "any(any#topdown.bad_speculation)", "as": "Bad speculation" }
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
        { "expr": "any(topdown.retiring)", "as": "Retiring" },
        { "expr": "any(topdown.backend_bound)", "as": "Backend bound" },
        { "expr": "any(topdown.frontend_bound)", "as": "Frontend bound" },
        { "expr": "any(topdown.bad_speculation)", "as": "Bad speculation" },
        { "expr": "any(topdown.branch_mispredict)", "as": "Branch mispredict" },
        { "expr": "any(topdown.machine_clears)", "as": "Machine clears" },
        { "expr": "any(topdown.frontend_latency)", "as": "Frontend latency" },
        { "expr": "any(topdown.frontend_bandwidth)", "as": "Frontend bandwidth" },
        { "expr": "any(topdown.memory_bound)", "as": "Memory bound" },
        { "expr": "any(topdown.core_bound)", "as": "Core bound" },
        { "expr": "any(topdown.ext_mem_bound)", "as": "External Memory" },
        { "expr": "any(topdown.l1_bound)", "as": "L1 bound" },
        { "expr": "any(topdown.l2_bound)", "as": "L2 bound" },
        { "expr": "any(topdown.l3_bound)", "as": "L3 bound" }
       ]
      },
      { "level": "cross", "select":
       [
        { "expr": "any(any#topdown.retiring)", "as": "Retiring" },
        { "expr": "any(any#topdown.backend_bound)", "as": "Backend bound" },
        { "expr": "any(any#topdown.frontend_bound)", "as": "Frontend bound" },
        { "expr": "any(any#topdown.bad_speculation)", "as": "Bad speculation" },
        { "expr": "any(any#topdown.branch_mispredict)", "as": "Branch mispredict" },
        { "expr": "any(any#topdown.machine_clears)", "as": "Machine clears" },
        { "expr": "any(any#topdown.frontend_latency)", "as": "Frontend latency" },
        { "expr": "any(any#topdown.frontend_bandwidth)", "as": "Frontend bandwidth" },
        { "expr": "any(any#topdown.memory_bound)", "as": "Memory bound" },
        { "expr": "any(any#topdown.core_bound)", "as": "Core bound" },
        { "expr": "any(any#topdown.ext_mem_bound)", "as": "External Memory" },
        { "expr": "any(any#topdown.l1_bound)", "as": "L1 bound" },
        { "expr": "any(any#topdown.l2_bound)", "as": "L2 bound" },
        { "expr": "any(any#topdown.l3_bound)", "as": "L3 bound" }
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
         "CPU_CLK_THREAD_UNHALTED:THREAD_P,UOPS_RETIRED:RETIRE_SLOTS,UOPS_ISSUED:ANY,INT_MISC:RECOVERY_CYCLES,IDQ_UOPS_NOT_DELIVERED:CORE"
     },
     "query"  :
     [
      { "level": "local", "select":
       [
        { "expr": "inclusive_sum(sum#papi.CPU_CLK_THREAD_UNHALTED:THREAD_P)", "as": "cpu_clk_thread_unhalted:thread_p" },
        { "expr": "inclusive_sum(sum#papi.UOPS_RETIRED:RETIRE_SLOTS)",   "as": "uops_retired:retire_slots"    },
        { "expr": "inclusive_sum(sum#papi.UOPS_ISSUED:ANY)",             "as": "uops_issued:any"              },
        { "expr": "inclusive_sum(sum#papi.INT_MISC:RECOVERY_CYCLES)",    "as": "int_misc:recovery_cycles"     },
        { "expr": "inclusive_sum(sum#papi.IDQ_UOPS_NOT_DELIVERED:CORE)", "as": "idq_uops_note_delivered:core" }
       ]
      },
      { "level": "cross", "select":
       [
        { "expr": "sum(inclusive#sum#papi.CPU_CLK_THREAD_UNHALTED:THREAD_P)", "as": "cpu_clk_thread_unhalted:thread_p" },
        { "expr": "sum(inclusive#sum#papi.UOPS_RETIRED:RETIRE_SLOTS)",   "as": "uops_retired:retire_slots"    },
        { "expr": "sum(inclusive#sum#papi.UOPS_ISSUED:ANY)",             "as": "uops_issued:any"              },
        { "expr": "sum(inclusive#sum#papi.INT_MISC:RECOVERY_CYCLES)",    "as": "int_misc:recovery_cycles"     },
        { "expr": "sum(inclusive#sum#papi.IDQ_UOPS_NOT_DELIVERED:CORE)", "as": "idq_uops_note_delivered:core" }
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
         "BR_MISP_RETIRED:ALL_BRANCHES
          ,CPU_CLK_THREAD_UNHALTED:THREAD_P
          ,CYCLE_ACTIVITY:CYCLES_NO_EXECUTE
          ,CYCLE_ACTIVITY:STALLS_L1D_PENDING
          ,CYCLE_ACTIVITY:STALLS_L2_PENDING
          ,CYCLE_ACTIVITY:STALLS_LDM_PENDING
          ,IDQ_UOPS_NOT_DELIVERED:CORE
          ,IDQ_UOPS_NOT_DELIVERED:CYCLES_0_UOPS_DELIV_CORE
          ,INT_MISC:RECOVERY_CYCLES
          ,MACHINE_CLEARS:COUNT
          ,MEM_LOAD_UOPS_RETIRED:L3_HIT
          ,MEM_LOAD_UOPS_RETIRED:L3_MISS
          ,UOPS_EXECUTED:CORE_CYCLES_GE_1
          ,UOPS_EXECUTED:CORE_CYCLES_GE_2
          ,UOPS_ISSUED:ANY
          ,UOPS_RETIRED:RETIRE_SLOTS"
     },
     "query"  :
     [
      { "level": "local", "select":
       [
        { "expr": "inclusive_sum(sum#papi.BR_MISP_RETIRED:ALL_BRANCHES)", "as": "br_misp_retired:all_branches" },
        { "expr": "inclusive_sum(sum#papi.CPU_CLK_THREAD_UNHALTED:THREAD_P)", "as": "cpu_clk_thread_unhalted:thread_p" },
        { "expr": "inclusive_sum(sum#papi.CYCLE_ACTIVITY:CYCLES_NO_EXECUTE)", "as": "cycle_activity:cycles_no_execute" },
        { "expr": "inclusive_sum(sum#papi.CYCLE_ACTIVITY:STALLS_L1D_PENDING)", "as": "cycle_activity:stalls_l1d_pending" },
        { "expr": "inclusive_sum(sum#papi.CYCLE_ACTIVITY:STALLS_L2_PENDING)", "as": "cycle_activity:stalls_l2_pending" },
        { "expr": "inclusive_sum(sum#papi.CYCLE_ACTIVITY:STALLS_LDM_PENDING)", "as": "cycle_activity:stalls_ldm_pending" },
        { "expr": "inclusive_sum(sum#papi.IDQ_UOPS_NOT_DELIVERED:CORE)", "as": "idq_uops_note_delivered:core" },
        { "expr": "inclusive_sum(sum#papi.IDQ_UOPS_NOT_DELIVERED:CYCLES_0_UOPS_DELIV_CORE)", "as": "idq_uops_note_delivered:cycles_0_uops_deliv_core" },
        { "expr": "inclusive_sum(sum#papi.INT_MISC:RECOVERY_CYCLES)", "as": "int_misc:recovery_cycles"     },
        { "expr": "inclusive_sum(sum#papi.MACHINE_CLEARS:COUNT)", "as": "machine_clears:count" },
        { "expr": "inclusive_sum(sum#papi.MEM_LOAD_UOPS_RETIRED:L3_HIT)", "as": "mem_load_uops_retired:l3_hit" },
        { "expr": "inclusive_sum(sum#papi.MEM_LOAD_UOPS_RETIRED:L3_MISS)", "as": "mem_load_uops_retired:l3_miss" },
        { "expr": "inclusive_sum(sum#papi.UOPS_EXECUTED:CORE_CYCLES_GE_1)", "as": "uops_executed:core_cycles_ge_1" },
        { "expr": "inclusive_sum(sum#papi.UOPS_EXECUTED:CORE_CYCLES_GE_2)", "as": "uops_executed:core_cycles_ge_2" },
        { "expr": "inclusive_sum(sum#papi.UOPS_ISSUED:ANY)",             "as": "uops_issued:any"              },
        { "expr": "inclusive_sum(sum#papi.UOPS_RETIRED:RETIRE_SLOTS)",   "as": "uops_retired:retire_slots"    }
       ]
      },
      { "level": "cross", "select":
       [
        { "expr": "sum(inclusive#sum#papi.BR_MISP_RETIRED:ALL_BRANCHES)", "as": "br_misp_retired:all_branches" },
        { "expr": "sum(inclusive#sum#papi.CPU_CLK_THREAD_UNHALTED:THREAD_P)", "as": "cpu_clk_thread_unhalted:thread_p" },
        { "expr": "sum(inclusive#sum#papi.CYCLE_ACTIVITY:CYCLES_NO_EXECUTE)", "as": "cycle_activity:cycles_no_execute" },
        { "expr": "sum(inclusive#sum#papi.CYCLE_ACTIVITY:STALLS_L1D_PENDING)", "as": "cycle_activity:stalls_l1d_pending" },
        { "expr": "sum(inclusive#sum#papi.CYCLE_ACTIVITY:STALLS_L2_PENDING)", "as": "cycle_activity:stalls_l2_pending" },
        { "expr": "sum(inclusive#sum#papi.CYCLE_ACTIVITY:STALLS_LDM_PENDING)", "as": "cycle_activity:stalls_ldm_pending" },
        { "expr": "sum(inclusive#sum#papi.IDQ_UOPS_NOT_DELIVERED:CORE)", "as": "idq_uops_note_delivered:core" },
        { "expr": "sum(inclusive#sum#papi.IDQ_UOPS_NOT_DELIVERED:CYCLES_0_UOPS_DELIV_CORE)", "as": "idq_uops_note_delivered:cycles_0_uops_deliv_core" },
        { "expr": "sum(inclusive#sum#papi.INT_MISC:RECOVERY_CYCLES)", "as": "int_misc:recovery_cycles"     },
        { "expr": "sum(inclusive#sum#papi.MACHINE_CLEARS:COUNT)", "as": "machine_clears:count" },
        { "expr": "sum(inclusive#sum#papi.MEM_LOAD_UOPS_RETIRED:L3_HIT)", "as": "mem_load_uops_retired:l3_hit" },
        { "expr": "sum(inclusive#sum#papi.MEM_LOAD_UOPS_RETIRED:L3_MISS)", "as": "mem_load_uops_retired:l3_miss" },
        { "expr": "sum(inclusive#sum#papi.UOPS_EXECUTED:CORE_CYCLES_GE_1)", "as": "uops_executed:core_cycles_ge_1" },
        { "expr": "sum(inclusive#sum#papi.UOPS_EXECUTED:CORE_CYCLES_GE_2)", "as": "uops_executed:core_cycles_ge_2" },
        { "expr": "sum(inclusive#sum#papi.UOPS_ISSUED:ANY)",             "as": "uops_issued:any"              },
        { "expr": "sum(inclusive#sum#papi.UOPS_RETIRED:RETIRE_SLOTS)",   "as": "uops_retired:retire_slots"    }
       ]
      }
     ]
    },
    {
     "name"        : "output",
     "description" : "Output location ('stdout', 'stderr', or filename)",
     "type"        : "string",
     "category"    : "output"
    },
    {
     "name"        : "adiak.import_categories",
     "services"    : [ "adiak_import" ],
     "description" : "Adiak import categories. Comma-separated list of integers.",
     "type"        : "string",
     "category"    : "adiak"
    },
    {
     "name"        : "max_column_width",
     "type"        : "int",
     "description" : "Maximum column width in the tree display",
     "category"    : "treeformatter"
    },
    {
     "name"        : "print.metadata",
     "type"        : "bool",
     "description" : "Print program metadata (Caliper globals and Adiak data)",
     "category"    : "treeformatter"
    }
    ]
)json";

}
