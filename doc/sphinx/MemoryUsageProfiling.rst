Memory usage profiling
================================

Caliper provides basic functionality to profile memory usage on Linux systems.

Heap allocation statistics
--------------------------------

The `mem.highwatermark` option reports the high-water mark of memory
allocations on the heap. To do so Caliper intercepts all malloc and free operations,
so enabling this option can be somewhat expensive for programs with many small
memory allocations.

The output is the heap memory high-watermark observed in each region: ::

    $ CALI_CONFIG=runtime-report,mem.highwatermark ./lulesh2.0
    [...]
    Path                                       Time (E) Time (I) Time % (E) Time % (I) Allocated MB
    main                                       0.008663 8.875162   0.097580  99.974940     8.758650
      lulesh.cycle                             0.000111 8.866499   0.001250  99.877359     8.748042
        TimeIncrement                          0.000051 0.000051   0.000575   0.000575     8.748042
        LagrangeLeapFrog                       0.000183 8.866337   0.002066  99.875535     8.748042
          LagrangeNodal                        0.085510 0.260078   0.963229   2.929667     8.748042
            CalcForceForNodes                  0.021734 0.174568   0.244825   1.966438     8.748042
              CalcVolumeForceForElems          0.033417 0.152834   0.376430   1.721613     9.612042
                IntegrateStressForElems        0.046602 0.046602   0.524950   0.524950    14.796042
                CalcHourglassControlForElems   0.024982 0.072815   0.281411   0.820232    19.980042
                  CalcFBHourglassForceForElems 0.047833 0.047833   0.538821   0.538821    25.164042
          LagrangeElements                     0.017402 8.028574   0.196021  90.438482     8.748042
    [...]

Note that `mem.highwatermark` *only* accounts for memory allocations through C
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

Unlike the `mem.highwatermark` option this feature does not require intercepting
malloc/free calls, so it should induce less runtime overhead in programs using many
small memory allocations. It does however induce a medium per-region overhead for
reading information from `/proc`. Therefore, it may be advisable to limit profiling
to high-level regions (e.g. with `level=phase` if the target code distinguishes
regular and phase regions).

Also unlike the `mem.highwatermark` option, `mem.pages` captures all memory usage 
(except GPUs) including code segments, static memory, etc. However, according to
the Linux documentation, the reported numbers may not be entirely accurate.

The option adds `VmSize`, `VmRSS`, and `Data` metrics representing the high-water
mark in number of pages observed in the respective categories. Note the number of
pages in the example is highest in the `CalcFBHourglassForceForElems` function,
just as in the `mem.highwatermark` output before: ::

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
