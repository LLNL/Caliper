# CUDA Unified Memory Analysis

The `cuptitrace` service can trace CUDA unified memory events (device-to-host and host-to-device data transfers, CPU pagefaults, and GPU pagefaults).
Enable it by adding "uvm" to CALI_CUPTITRACE_ACTIVITIES.
Notes:

* Applications must call ``cuInit(0)`` before initializing Caliper to use UVM tracking.

## Memory region tracking

Caliper can map UVM events to memory regions using Caliper's memory tracking feature.
This requires

* Annotating memory regions with Caliper's data tracking API
* Enabling the alloc service "resolve address" feature
* Using the cuptitrace "flush-on-snapshot" mode

### Memory annotations

Place track/untrack calls around memory allocations:

```C++
    #include <caliper/cali_datatracker.h>

    cudaMallocManaged(&ptr, size);
    cali_datatracker_track(ptr, "label", size);
    // ...
    cali_datatracker_untrack(ptr);
    cudaFree(ptr);
```

### Configuration

CUpti activity tracing writes asynchronous event traces into a Caliper-managed buffer.
Normally, we flush this buffer at program exit directly into the final output stream.
For memory allocation tracking, we use the cuptitrace "flush-on-snapshot" mode instead, which flushes the CUpti buffer intermittently and forwards the CUpti activities as snapshots to other Caliper services.
The alloc service then maps CUpti pagefault addresses to labelled memory regions.

By default, CUpti buffer flushes are triggered via the "cupti.sync" attribute at CUDA sync events.
The cupti service must therefore be enabled as well.

Example configuration:

    CALI_SERVICES_ENABLE=alloc,cupti,cuptitrace,trace,recorder
    CALI_ALLOC_RESOLVE_ADDRESSES=true
    CALI_CUPTI_CALLBACK_DOMAINS=sync
    CALI_CUPTITRACE_ACTIVITIES=uvm
    CALI_CUPTITRACE_CORRELATE_CONTEXT=false
    CALI_CUPTITRACE_FLUSH_ON_SNAPSHOT=true

This should record a trace with all UVM events and map memory regions.
Notes:

* With flush-on-snapshot mode, CUpti activity snapshots contain the Caliper region where the flush happened. Context correlation (mapping CUpti activites to regions where they originate) won't work.
* Use flush-on-snapshot mode with care - UVM region tracking is pretty much the only useful use case.

### Output

Trace output from the config above has the following relevant attributes:

* cupti.uvm.kind
    * UVM activity kind, e.g. transfer or page fault. (DtoH, HtoD, pagefault.gpu, pagefault.cpu)
* cupti.fault.addr
    * Page address of the unified memory event.
* alloc.label#cupti.fault.addr
    * Resolved user annotation label for the unified memory event's page address.
* cupti.uvm.bytes
    * Bytes involved in a UVM data transfer
* cupti.activity.duration
    * Time in ns for the activity.

Example query:

    $ cali-query -q "select cupti.uvm.kind as \"UVM Activity\",alloc.label#cupti.fault.addr as Label,count(),scale(cupti.uvm.bytes,1e-6) as MB,scale(cupti.activity.duration,1e-9) as \"Time (s)\" group by cupti.uvm.kind,alloc.label#cupti.fault.addr where cupti.uvm.kind format table" <trace.cali>

Example output from a modified Rodinia CUDA BFS benchmark:

    UVM Activity   Label                 count MB        Time (s)
    pagefaults.gpu h_updating_graph_mask    10           0.001716
    pagefaults.gpu h_graph_nodes             8           0.005155
    pagefaults.gpu h_cost                    7           0.000553
    pagefaults.gpu h_graph_visited           6           0.000838
    pagefaults.gpu h_graph_edges             6           0.002730
    pagefaults.gpu h_graph_mask              4           0.000802
    pagefaults.cpu h_graph_edges            71
    pagefaults.cpu h_graph_nodes            24
    pagefaults.cpu h_updating_graph_mask    15
    pagefaults.cpu h_cost                   12
    pagefaults.cpu h_graph_mask              6
    pagefaults.cpu h_graph_visited           5
    HtoD           h_graph_edges           237 24.051712 0.001303
    HtoD           h_graph_nodes            74  8.060928 0.000426
    HtoD           h_cost                   47  4.063232 0.000265
    HtoD           h_graph_visited          14  1.048576 0.000071
    HtoD           h_updating_graph_mask    14  1.769472 0.000088
    HtoD           h_graph_mask              8  1.048576 0.000049
    DtoH           h_updating_graph_mask    11  0.720896 0.000036
