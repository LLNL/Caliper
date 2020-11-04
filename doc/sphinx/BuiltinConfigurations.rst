Built-in profiling configurations
=========================================

Caliper includes built-in configurations for many common performance analysis
tasks. These configurations can be enabled through the :doc:`ConfigManagerAPI`
or the :doc:envvar:`CALI_CONFIG` environment variable.

Configuration String Syntax
-----------------------------------------

A configuration string for the `ConfigManager` class or `CALI_CONFIG` environment
variable is a comma-separated list of *configs* and *parameters*.

A *config* is the name of one of Caliper's built-in measurement configurations,
e.g. `runtime-report` or `event-trace`. Multiple configs can be specified,
separated by comma.

Most configs have optional parameters, e.g. `output` to name an output file.
Parameters can be specified as a list of key-value pairs in parentheses after the
config name, e.g. ``runtime-report(output=report.txt,io.bytes=true)``. For boolean
parameters, only the key needs to be added to enable it; for example,
`io.bytes` is equal to `io.bytes=true`.

Parameters can also be listed separately in the config string, outside of parentheses.
In that case, the parameter applies to *all* configs, whereas parameters inside
parentheses only apply to the  config where they are listed. For example, in
``runtime-report(io.bytes),spot,mem.highwatermark``, the `mem.highwatermark` option
will be active in both the runtime-report and spot config, whereas `io.bytes`
will only be active for runtime-report. Configs and parameters can be listed in
any order.

Here is a more complex example::

    runtime-report(output=stdout),profile.cuda,mem.highwatermark,event-trace(output=trace.cali,trace.io)

This will print a runtime profile to stdout, including CUDA API calls and memory
high-water marks in the profile, and write an event trace with region begin/end and I/O
operations into the trace.cali file.

Built-in configs
-------------------------------

The following list describes the ConfigManager's built-in configs and their
parameters. Note that depending on the Caliper build configuration, not all
configs or options may be available in a particular Caliper installation.

event-trace
   Record a trace of region enter/exit events in .cali format. Options:

   event.timestamps
      Record event timestamps
   output
      Output location ('stdout', 'stderr', or filename)
   trace.io
      Trace I/O events
   trace.mpi
      Trace I/O events

hatchet-region-profile
   Record a region time profile for processing with hatchet. Options:

   adiak.import_categories
      Adiak import categories. Comma-separated list of integers.
   io.bytes
      Report I/O bytes written and read
   io.bytes.read
      Report I/O bytes read
   io.bytes.written
      Report I/O bytes written
   io.read.bandwidth
      Report I/O read bandwidth
   io.write.bandwidth
      Report I/O write bandwidth
   mem.highwatermark
      Record memory high-water mark for regions
   output
      Output location ('stdout', 'stderr', or filename)
   output.format
      Output format ('hatchet', 'cali', 'json')
   profile.cuda
      Profile CUDA API functions
   profile.mpi
      Profile MPI functions
   topdown-counters.all
      Raw counter values for Intel top-down analysis (all levels)
   topdown-counters.toplevel
      Raw counter values for Intel top-down analysis (top level)
   topdown.all
      Top-down analysis for Intel CPUs (all levels)
   topdown.toplevel
      Top-down analysis for Intel CPUs (top level)
   use.mpi
      Merge results into a single output stream in MPI programs

hatchet-sample-profile
   Record a sampling profile for processing with hatchet. Options:

   adiak.import_categories
      Adiak import categories. Comma-separated list of integers.
   lookup.module
      Lookup source module (.so/.exe)
   lookup.sourceloc
      Lookup source location (file+line)
   output
      Output location ('stdout', 'stderr', or filename)
   output.format
      Output format ('hatchet', 'cali', 'json')
   sample.callpath
      Perform call-stack unwinding
   sample.frequency
      Sampling frequency in Hz. Default: 200
   use.mpi
      Merge results into a single output stream in MPI programs

callpath-sample-report
   Print a call-path sampling profile for the program

   aggregate_across_ranks
      Aggregate results across MPI ranks
   io.bytes
      Report I/O bytes written and read
   io.bytes.read
      Report I/O bytes read
   io.bytes.written
      Report I/O bytes written
   io.read.bandwidth
      Report I/O read bandwidth
   io.write.bandwidth
      Report I/O write bandwidth
   max_column_width
      Maximum column width in the tree display
   mem.highwatermark
      Record memory high-water mark for regions
   output
      Output location ('stdout', 'stderr', or filename)
   sample.frequency
      Sampling frequency in Hz. Default: 200
   topdown-counters.all
      Raw counter values for Intel top-down analysis (all levels)
   topdown-counters.toplevel
      Raw counter values for Intel top-down analysis (top level)
   topdown.all
      Top-down analysis for Intel CPUs (all levels)
   topdown.toplevel
      Top-down analysis for Intel CPUs (top level)

loop-report
   Print summary and time-series information for loops. Options:

   aggregate_across_ranks
      Aggregate results across MPI ranks
   io.bytes
      Report I/O bytes written and read
   io.bytes.read
      Report I/O bytes read
   io.bytes.written
      Report I/O bytes written
   io.read.bandwidth
      Report I/O read bandwidth
   io.write.bandwidth
      Report I/O write bandwidth
   iteration_interval
      Measure every N loop iterations
   mem.highwatermark
      Record memory high-water mark for regions
   output
      Output location ('stdout', 'stderr', or filename)
   summary
      Print loop summary
   target_loops
      List of loops to target. Default: any top-level loop.
   time_interval
      Measure after t seconds
   timeseries
      Print time series
   timeseries.maxrows
      Max number of rows in timeseries display. Set to 0 to show all. Default: 20.
   topdown.all
      Top-down analysis for Intel CPUs (all levels)
   topdown.toplevel
      Top-down analysis for Intel CPUs (top level)

mpi-report
   Print time spent in MPI functions

runtime-report
   Print a time profile for annotated regions. Options:

   aggregate_across_ranks
      Aggregate results across MPI ranks
   calc.inclusive
      Report inclusive instead of exclusive times
   io.bytes
      Report I/O bytes written and read
   io.bytes.read
      Report I/O bytes read
   io.bytes.written
      Report I/O bytes written
   io.read.bandwidth
      Report I/O read bandwidth
   io.write.bandwidth
      Report I/O write bandwidth
   max_column_width
      Maximum column width in the tree display
   mem.highwatermark
      Record memory high-water mark for regions
   output
      Output location ('stdout', 'stderr', or filename)
   profile.cuda
      Profile CUDA API functions
   profile.mpi
      Profile MPI functions
   topdown.all
      Top-down analysis for Intel CPUs (all levels)
   topdown.toplevel
      Top-down analysis for Intel CPUs (top level)

spot
   Record a time profile for the Spot web visualization framework. Options:

   adiak.import_categories
      Adiak import categories. Comma-separated list of integers.
   aggregate_across_ranks
      Aggregate results across MPI ranks
   io.bytes
      Report I/O bytes written and read
   io.bytes.read
      Report I/O bytes read
   io.bytes.written
      Report I/O bytes written
   io.read.bandwidth
      Report I/O read bandwidth
   io.write.bandwidth
      Report I/O write bandwidth
   mem.highwatermark
      Record memory high-water mark for regions
   output
      Output location ('stdout', 'stderr', or filename)
   profile.cuda
      Profile CUDA API functions
   profile.mpi
      Profile MPI functions
   topdown-counters.all
      Raw counter values for Intel top-down analysis (all levels)
   topdown-counters.toplevel
      Raw counter values for Intel top-down analysis (top level)
   topdown.all
      Top-down analysis for Intel CPUs (all levels)
   topdown.toplevel
      Top-down analysis for Intel CPUs (top level)
