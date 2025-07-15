Memory usage profiling
================================

Caliper provides basic functionality to profile memory usage on Linux systems.

Heap allocation statistics
--------------------------------

The `alloc.stats` option reports heap memory allocation statistics.
To do so Caliper intercepts all malloc and free operations, so enabling
this option  can be somewhat expensive for programs with many small memory
allocations. The output contains the following metrics:

Mem HWM
    The memory high-water mark in bytes. Maximum total amount of memory that
    was allocated in this region.

Alloc tMax
    Allocation tally maximum. This is the maximum total amount of memory that was
    allocated directly by this region. In contrast, the memory high-water mark
    includes memory allocated in any region.

Alloc count
    Number of individual memory allocations.

Min, Avg, Max Bytes/alloc
    Minimum, average and maximum bytes allocated per allocation operation.

An example output looks like this: ::

    $ CALI_CONFIG=runtime-report,alloc.stats ./lulesh2.0
    Path                                       Time (E) Time (I) Time % (E) Time % (I) Mem HWM  Alloc tMax Alloc count Min Bytes/alloc Avg Bytes/alloc Max Bytes/alloc
    main                                       0.004867 0.100623   4.712664  97.423931  8750898    8750898          71               1          124779          864000
      lulesh.cycle                             0.000259 0.095755   0.250617  92.711267  8740194         96          10              96              96              96
        TimeIncrement                          0.000084 0.000084   0.081690   0.081690  8740194
        LagrangeLeapFrog                       0.000488 0.095412   0.472577  92.378960  8740194         96          20              96              96              96
          LagrangeNodal                        0.001119 0.060091   1.083415  58.180911  8740194
            CalcForceForNodes                  0.000490 0.058972   0.474139  57.097497  8740194        216           2              24             108             192
              CalcVolumeForceForElems          0.000710 0.058483   0.687355  56.623357  9604194     864000          40          216000          216000          216000
                IntegrateStressForElems        0.014498 0.014498  14.036874  14.036874 14788194    5184000          30         1728000         1728000         1728000
                CalcHourglassControlForElems   0.021265 0.043275  20.588633  41.899129 19972194   10368000          60         1728000         1728000         1728000
                  CalcFBHourglassForceForElems 0.022010 0.022010  21.310495  21.310495 25156194    5184000          30         1728000         1728000         1728000
          LagrangeElements                     0.000215 0.034296   0.207968  33.205399  8740194
            CalcLagrangeElements               0.000369 0.007101   0.357067   6.875254  9388194     648000          30          216000          216000          216000
              CalcKinematicsForElems           0.006732 0.006732   6.518187   6.518187  9388194
            CalcQForElems                      0.003196 0.005037   3.094518   4.877323 10165794    1425600          60          216000          237600          259200
              CalcMonotonicQForElems           0.001841 0.001841   1.782805   1.782805 10165794
            ApplyMaterialPropertiesForElems    0.000541 0.021942   0.524234  21.244854  8956194     216000          10          216000          216000          216000
              EvalEOSForElems                  0.007972 0.021401   7.718625  20.720620  9902034     945840        1540             984           19636           67560
                CalcEnergyForElems             0.013429 0.013429  13.001996  13.001996  9969594      67560         350             984           45988           67560

Note that `alloc.stats` *only* accounts for memory allocations through C
heap allocation calls (malloc, calloc, realloc, free) within the active profiling
session. It does *not* account for any memory allocated through other means,
including:

* Allocations that happened before initializing the Caliper profiling session
* Objects allocated in static memory or on the stack
* Text segments (i.e., program code)
* GPU memory allocated through `cudaMalloc` or `hipMalloc` etc.
* Any heap allocations through other means, such as calling `mmap` directly or
  CUDA/HIP host memory allocation calls unless they fall back to malloc/free

Memory page use statistics
--------------------------------

The `mem.pages` option provides high-water marks for total (VmSize), resident
set (VmRSS), and Data memory usage in number of pages, similar to memory
statistics reported by tools like `top`.

Unlike the `alloc.stats` option this feature does not require intercepting
malloc/free calls, so it should induce less runtime overhead in programs using many
small memory allocations. It does however induce a medium per-region overhead for
reading information from `/proc`. Therefore, it may be advisable to limit profiling
to high-level regions (e.g. with `level=phase` if the target code distinguishes
regular and phase regions).

Also unlike the `alloc.stats` option, `mem.pages` captures all memory usage
(except GPUs) including code segments, static memory, etc. However, according to
the Linux documentation, the reported numbers may not be entirely accurate.

The option adds `VmSize`, `VmRSS`, and `Data` metrics representing the high-water
mark in number of pages observed in the respective categories. Note the number of
pages in the example is highest in the `CalcFBHourglassForceForElems` function,
just as in the `alloc.stats` output before: ::

    $ CALI_CONFIG=runtime-report,mem.highwatermark ./lulesh2.0
    [...]
    Path                                       Time (E) Time (I) Time % (E) Time % (I) VmSize VmRSS Data
    main                                       0.010168 9.979632   0.101883  99.997550  79862  6786 33651
      lulesh.cycle                             0.000085 9.969464   0.000855  99.895667  81740  7665 35529
        TimeIncrement                          0.000047 0.000047   0.000469   0.000469  81740  7665 35529
        LagrangeLeapFrog                       0.000839 9.969332   0.008402  99.894343  81740  7665 35529
          LagrangeNodal                        0.056944 0.239477   0.570587   2.399594  81740  7665 35529
            CalcForceForNodes                  0.017880 0.182533   0.179165   1.829007  81740  7665 35529
              CalcVolumeForceForElems          0.039597 0.164652   0.396773   1.649842  81740  7742 35529
                IntegrateStressForElems        0.051738 0.051738   0.518423   0.518423  81740  7742 35529
                CalcHourglassControlForElems   0.030218 0.073317   0.302787   0.734645  83849  9804 37638
                  CalcFBHourglassForceForElems 0.043099 0.043099   0.431859   0.431859  84271 10196 38060
          LagrangeElements                     0.012675 9.057428   0.127006  90.756916  81740  7665 35529
