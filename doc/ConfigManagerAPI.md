# The Caliper ConfigManager API

Caliper provides several built-in performance measurement and reporting configurations. These can be activated from within a program with the `ConfigManager` API using a short configuration string. Configuration strings can be hard-coded in the program or provided by the user in some form, e.g. as a command-line parameter or in the programs's configuration file.

To access and control the built-in configurations, create a `cali::ConfigManager` object. Add a configuration string with `add()`, start the requested configuration channels with `start()`, and trigger output with `flush()`:

```C++
#include <caliper/cali-manager.h>

int main(int argc, char* argv[])
{
  cali::ConfigManager mgr;

  if (argc > 1)
    mgr.add(argv[1]);
  if (mgr.error())
    std::cerr << "Config error: " << mgr.error_msg() << std::endl;
  // ...
  mgr.start(); // start requested performance measurement channels
  // ... (program execution)
  mgr.flush(); // write performance results
}
```

A complete code example is [here](../examples/apps/cxx-example.cpp).

## ConfigManager Configuration String Syntax

A configuration string for the `ConfigManager` class is a comma-separated list of *configs* and *parameters*.

A *config* is the name of one of Caliper's built-in measurement configurations, e.g. `runtime-report` or `event-trace`. Multiple configs can be specified, separated by comma.

Most configs have optional parameters, e.g. `output` to name an output file. Parameters can be specified as a list of key-value pairs in parantheses after the config name, e.g. `runtime-report(output=report.txt,io.bytes=true)`. For boolean parameters, only the key needs to be added to enable it; for example, `io.bytes` is equal to `io.bytes=true`.

Parameters can also be listed separately in the config string, outside of parentheses. In that case, the parameter applies to *all* configs, whereas parameters inside parentheses only apply to the  config where they are listed. For example, in `runtime-report(io.bytes),spot,mem.highwatermark`, the `mem.highwatermark` option will be active in both the runtime-report and spot config, whereas `io.bytes` will only be active for runtime-report. Configs and parameters can be listed in any order.

Here is a more complex example:

    runtime-report(output=stdout),profile.cuda,mem.highwatermark,event-trace(output=trace.cali,trace.io)

This will print a runtime profile to stdout, including CUDA API calls and memory high-water marks in the profile, and write an event trace with region begin/end and I/O operations into the trace.cali file.

# Built-in Configs

The following sections describe the ConfigManager's built-in configs and their parameters.

## runtime-report

The runtime-report config prints a time profile for Caliper-annotated regions:

    Path                                         Inclusive time Exclusive time Time %
    main                                               0.149681       0.008896  5.830346
      lulesh.cycle                                     0.140785       0.000053  0.034736
        LagrangeLeapFrog                               0.140667       0.000050  0.032769
          CalcTimeConstraintsForElems                  0.004589       0.004589  3.007583
          LagrangeElements                             0.086606       0.000201  0.131733
            ApplyMaterialPropertiesForElems            0.078804       0.000377  0.247082
              EvalEOSForElems                          0.078427       0.014422  9.452029
                CalcEnergyForElems                     0.064005       0.064005 41.948211
            CalcQForElems                              0.004483       0.002415  1.582766
              CalcMonotonicQForElems                   0.002068       0.002068  1.355346
            CalcLagrangeElements                       0.003118       0.000460  0.301479
              CalcKinematicsForElems                   0.002658       0.002658  1.742026
          LagrangeNodal                                0.049422       0.011016  7.219772
            CalcForceForNodes                          0.038406       0.002194  1.437925
              CalcVolumeForceForElems                  0.036212       0.000691  0.452874
                CalcHourglassControlForElems           0.023037       0.010822  7.092626
                  CalcFBHourglassForceForElems         0.012215       0.012215  8.005584
                IntegrateStressForElems                0.012484       0.012484  8.181884
        TimeIncrement                                  0.000065       0.000065  0.042600

By default, runtime-report collects a time profile and writes it to stderr on `flush()`. For MPI programs, it aggregates results across ranks and prints statistics.

| Parameter name         | Description                                     |
|------------------------|-------------------------------------------------|
| aggregate_across_ranks | Enable/disable aggregation across ranks in MPI programs |
| output                 | Output file name, or stderr/stdout |
| mem.highwatermark      | Record and print memory high-water mark in annotated regions |
| io.bytes               | Record and print I/O bytes written and read in annotated regions |
| io.bandwidth           | Record and print I/O bandwidth in annotated regions |
| profile.mpi            | Profile MPI functions |
| profile.cuda           | Profile host-side CUDA API functions (e.g, cudaMalloc) |

## event-trace

The event-trace config records a trace of region enter/exit events and writes it out in Caliper's .cali format for processing with Caliper's cali-query tool. It generates a unique output file name by default. Every process writes a separate file.

| Parameter name         | Description                                     |
|------------------------|-------------------------------------------------|
| event.timestamps       | Add time since program start in the event records |
| output                 | Output file name, or stderr/stdout |
| trace.mpi              | Trace MPI functions |
| trace.cuda             | Trace host-side CUDA API functions (e.g, cudaMalloc) |
| trace.io               | Trace I/O |

## nvprof

The nvprof config forwards Caliper annotations to NVidia's NVProf profiler. It generates no output on its own.

## spot

The spot config records a time profile and writes a .cali file to be processed by the Spot web visualization framework.

| Parameter name         | Description                                     |
|------------------------|-------------------------------------------------|
| aggregate_across_ranks | Enable/disable aggregation across ranks in MPI programs |
| output                 | Output file name, or stderr/stdout |
| mem.highwatermark      | Record and print memory high-water mark in annotated regions |
| io.bytes               | Record and print I/O bytes written and read in annotated regions |
| io.bandwidth           | Record and print I/O bandwidth in annotated regions |

## hatchet-region-profile

The hatchet-region-profile config collects per-process region time profiles meant for processing with the [hatchet](https://github.com/LLNL/hatchet) library. By default, it will write a json file that hatchet can read. It can also write Caliper .cali files for processing with Caliper's `cali-query` tool.

| Parameter name         | Description                                     |
|------------------------|-------------------------------------------------|
| output                 | Output file name, or stderr/stdout |
| output.format          | Output format. 'json-split', 'json', or 'cali'. Default: 'json-split'. |
| io.bytes               | Record and print I/O bytes written and read in annotated regions |
| profile.mpi            | Profile MPI functions |
| profile.cuda           | Profile host-side CUDA API functions (e.g, cudaMalloc) |

## hatchet-sample-profile

The hatchet-sample-profile config performs call-path sampling to collect per-thread call-path and region sample profiles. By default, it writes json files for processing with the [hatchet](https://github.com/LLNL/hatchet) library. Profiles contain the sample count per region/call path.

Most options for the hatchet-sample-profile config require that Caliper is built with dyninst support.

| Parameter name         | Description                                     |
|------------------------|-------------------------------------------------|
| output                 | Output file name, or stderr/stdout |
| output.format          | Output format. 'json-split', 'json', or 'cali'. Default: 'json-split'. |
| sample.frequency       | Sampling frequency in Hz. Default: 200. |
| sample.threads         | Sample each thread. Default: true. |
| sample.callpath        | Perform call-stack lookup at each sample. Default: true. |
| lookup.module          | Lookup the module names (.so/exe) where samples were taken. |
| lookup.sourceloc       | Lookup the source file/line number where samples were taken. |

## cuda-activity

The cuda-activity config records CUDA activities (kernel execution, memory copies, etc.), and prints a region profile of time spent on the GPU as well as on the host. GPU activity time is assigned to the Caliper-annotated regions on the host where the activity was launched. Requires CUPTI support. The config performs tracing to record GPU activities, it is therefore not recommended for long-running executions.  Example output:

    Path                              Avg Host Time Max Host Time Avg GPU Time Max GPU Time GPU %
    main                                   1.614494      1.615764     0.693623     0.836652 42.962240
      qs.mainloop                          1.614295      1.615514     0.693623     0.836652 42.967531
        cycleFinalize                      0.001150      0.001530
        cycleTracking                      1.312167      1.380361     0.693623     0.836652 52.860857
          cycleTracking_Test_Done          0.000161      0.000162
          cycleTracking_MPI                0.285434      0.313542
            cycleTracking_Test_Done        0.019785      0.036499
          cycleTracking_Kernel             0.841515      0.917650     0.693623     0.836652 82.425458
        cycleInit                          0.296091      0.384112

Avg/Max Host and GPU time are average and maximum inclusive runtime for host and GPU activities, respectively, over all MPI ranks. "GPU %" is the ratio of GPU time to Host runtime in percent.

| Parameter name         | Description                                     |
|------------------------|-------------------------------------------------|
| aggregate_across_ranks | Enable/disable aggregation across ranks in MPI programs |
| output                 | Output file name, or stderr/stdout |
| profile.mpi            | Profile MPI functions |
| profile.cuda           | Profile host-side CUDA API functions (e.g, cudaMalloc) |
