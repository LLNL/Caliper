Top-down microarchitecture analysis
=======================================

The top-down microarchitecture analysis method is a hierarchical organization of event-based metrics that identifies the dominant performance bottlenecks in an application. Its aim is to show, on average, how well the CPU's pipeline(s) were being utilized while running an application `[1] https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2025-4/top-down-microarchitecture-analysis-method.html`_.

Caliper supports top-down analysis on Linux for recent Intel CPUs via PAPI or via Linux perf:

* The PAPI-based method requires PAPI 7.2 or higher with the `topdown` component enabled. Note that PAPI 7.2.0 has a bug that cause the top-down values to converge to a whole-program average rather than per-region values.
* The Linux perf based method is the preferred approach. It requires Caliper to be built with the ``-DWITH_LIBPFM=On`` switch or the ``+libpfm`` Spack variant.

Caliper can collect the level 1 or the level 1 and 2 counters of the top-down hierarchy. Use the ``topdown.toplevel`` ConfigManager option to collect level 1 counters only, or ``topdown.all`` to collect level 1 and level 2 counters. These options are available for profiling configs such as `runtime-report`, `runtime-profile`, and `spot`. The ``topdown.`` options use the Linux perf method by default, and PAPI when Linux perf is not available. One can also select the perf or PAPI methods explicitly with ``papi.topdown.toplevel`` or ``perf.topdown.toplevel``, respectively. Additionally, the ``event-trace`` config has the ``perf.topdown.toplevel`` and ``perf.topdown.all`` options to collect top-down counters when tracing.

The top-down values represent percentages indicating how the CPU's pipelines were utilized while running the code. The four level 1 values should add up to about 100% (see :ref:`notes` below). The values are:

Retiring
  Pipeline slots filled with retired instructions

Bad spec (bad speculation)
  Pipeline slots filled with discarded instructions due to bad speculation

FE bound (frontend bound)
  Pipeline stalled due to processor front-end bottlenecks

BE bound (backend bound)
  Pipeline stalled due to processor back-end bottlenecks

This example output for level 1 top-down values shows two computational kernels, `triad` and `matmul_f64`. We can see that `triad` is about 69% backend-bound, while `matmul_f64` is 81% retiring::

    $ CALI_CONFIG=runtime-report,topdown.toplevel
    Path           Time (E) Time (I) Time % (E) Time % (I) Retiring  Bad spec  FE bound  BE bound
    main                    4.522646            100.000000
      mainloop     0.021080 4.522646   0.466093 100.000000 19.600465 13.328316 48.609153 19.208456
        triad      0.514245 0.514245  11.370446  11.370446 16.946897  1.109167 12.546254 69.689925
        matmul_f64 3.987321 3.987321  88.163460  88.163460 81.568627  5.686140 11.667064  1.568627

The level 2 metrics further divide the level 1 metrics into the following categories:

Heavy ops
  Retired instructions with heavy operations

Light ops
  Retired instructions with light operations

Br mispr (branch mispredict)
  Bad speculation slots due to branch misprediction

Mach clrs (machine clears)
  Bad speculation slots due to machine clears

Fetch lat (fetch latency)
  Front-end bottlenecks due to fetch latency

Fetch BW (fetch bandwidth)
  Front-end bottlenecks due to fetch bandwidth

Core bound
  Core-bound backend bottlenecks

Mem bound
  Memory-bound backend bottlenecks

See [1] for details.

.. _notes:

Notes and caveats
---------------------------------------

Keep the following in mind when designing experiments using the top-down microarchitecture analysis:

* Avoid too much aggregation, e.g. running many iterations. In profiling configs, the top-down results are averaged over all region invocations and thus lose accuracy. If, for example, a region exhibits different behaviors with different inputs, this may dilute the final result. The top-down values may thus not add up to exactly 100%. This is especially pronounced with the PAPI method, which averages the top-down percentages equally across all region invocations regardless of length, i.e. a 0.1 second instance will have the same weight as a 2 second instance. The perf-based method scales results by the total number of slots and is less susceptible to this problem. Still, consider tracing if a region's behavior may vary substantially between invocations.
* The top-down microarchitecture analysis is fundamentally a single-thread performance analysis, so run single-thread or at least single-process experiments. In MPI mode, results are averaged over all processes and thus diluted.
* Avoid too short or too long running regions for best results. Ideally, aim for regions running between about 0.5 milliseconds to a few seconds. The perf topdown service does not record values for regions that are too short (less than ~50 microseconds).
* The top-down analysis only works on Ice Lake and later Intel CPUs. The level 2 counters only work on Sapphire Rapids and later Intel CPUs. The perf topdown service quits if it detects a non-Intel CPU, but it does not check the exact CPU model. It may segfault when trying to run on an unsupported CPU.
* On Intel CPUs with separate P(erformance) and E(fficiency) cores, the analysis likely only works on P cores. The perf-based method may segfault when running on E cores. In this case, try to pin the application thread(s) only to P cores.

References
---------------------------------------

[1] `Top-down Microarchitecture Analysis Method https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2025-4/top-down-microarchitecture-analysis-method.html`_