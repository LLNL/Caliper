Sample profiling
================================================================

Sampling is an easy way to provide additional detail over Caliper's source-code
region annotations by showing where within a region the program spends its time.
Sampling is currently available for Linux. It requires the
`-DWITH_SAMPLING=On`, `-DWITH_LIBDW=On`, and (optional)
`-DWITH_LIBUNWIND=On` options when building Caliper.

The `sample-report` configuration prints a sampling report with the time
spent in the sampled source-code functions within each Caliper region. For the
`cxx-example` program, we usually find that almost all of its time is spent
in the ``__nanosleep`` system function::

    $ CALI_CONFIG=sample-report ./examples/apps/cxx-example
    Path       Samples Time (sec) Function
    main
      mainloop
        foo         71   0.355000 __nanosleep

There are several options for `sample-report`:

source.function
    Print the function symbol name where samples hit (on by default)

source.location
    Print the source file name and line number where samples hit

source.module
    Print the source module (.so/.exe) where samples hit

callpath
    Print the function call path instead of the Caliper region
    hierarchy.

sample.frequency
    The sample frequency. The default is 200Hz (one sample every 5
    milliseconds). Increasing the sampling frequency provide more detail
    at the cost of higher overheads and processing times.

As an example, `source.location` shows us source file and line numbers
of sampled locations::

    $ CALI_CONFIG=sample-report,source.function=false,source.location ./examples/apps/cxx-example
    Path       Samples Time (sec) Source
    main
      mainloop
       |-            1   0.005000 ../src/caliper/Caliper.cpp:992
       |-            1   0.005000 ../include/caliper/comm~~ataAccessInterface.h:22
        foo         66   0.330000 ../sysdeps/unix/syscall-template.S:81

For MPI programs, `sample-report` prints statistics aggregated across MPI
ranks, just like `runtime-report`::

    $ CALI_CONFIG=sample-report srun -n 8 lulesh2.0
    Path         Min time/rank Max time/rank Avg time/rank Total time Time %    Function
    main
     |-               0.005000      0.005000      0.005000   0.035000  0.059691 Domain::AllocateElemPersistent
     |-               0.005000      0.005000      0.005000   0.035000  0.059691 Domain::SetupThreadSupportStru
     |-               0.005000      0.005000      0.005000   0.005000  0.008527 sysmalloc
     |-               0.005000      0.005000      0.005000   0.005000  0.008527 Domain::BuildMesh(int, int, in
    lulesh.cycle
        TimeIncrement
         |-           0.075000      0.740000      0.355000   2.840000  4.843523 gomp_barrier_wait_end
         |-           0.005000      0.060000      0.027857   0.195000  0.332566 psm2_mq_ipeek2
         |-           0.005000      0.005000      0.005000   0.015000  0.025582 psm_no_lock
         |-           0.005000      0.060000      0.023571   0.165000  0.281402 psm_progress_wait
         |-           0.015000      0.030000      0.022143   0.155000  0.264347 mv2_shm_bcast
         |-           0.005000      0.025000      0.013750   0.055000  0.093801 amsh_poll
         |-           0.005000      0.010000      0.007500   0.030000  0.051164 psmi_poll_internal
    ...

The `hatchet-sample-profile` config recipe writes machine-readable profiles for
processing with Hatchet or cali-query. The profile contains sampling data for each
MPI rank. By default, it writes a JSON file for processing with Hatchet, but you
can use `output.format=cali` to write a .cali file for processing with cali-query,
the Caliper Python reader library, or Hatchet's .cali importer.
`hatchet-sample-profile` supports the same options as `sample-report` above.

With `output.format=cali` option, the .cali file contains records with the
following attributes:

count
    Sample count metric (number of times a sample hit)

scount
    Scaled count metric: sample count times sample frequency. Represents
    time in seconds.

source.function#cali.sampler.pc
    Sampled function symbol name, if enabled with the source.function
    option

source.module#cali.sampler.pc
    Sampled module name, if enabled with the source.module option

sourceloc#cali.sampler.pc
    Sampled source file name + line number, if enabled with the
    source.location option

source.function#callpath.address
    The function call path, if enabled with the callpath option

In this example, we collect a sampling profile in .cali format and use
cali-query to compute a flat profile with the time and number of samples
in each sampled function::

    $ CALI_CONFIG=hatchet-sample-profile,source.function,output.format=cali ./lulesh2.0
    $ cat << EOF > query.txt
    > select
    >   source.function#cali.sampler.pc as Function,
    >   sum(count) as Samples,
    >   sum(scount) as Time,
    >   percent_total(count) as Percent
    > group by
    >   source.function#cali.sampler.pc
    > format
    >   table 
    > order by
    >   sum#count DESC  
    > EOF
    $ cali-query -Q query.txt sample_profile.cali
    Function                                                     Samples Time       Percent
    gomp_barrier_wait_end                                          62311 311.555000 26.918873
    CalcFBHourglassForceForElems(~~ int, int) [clone ._omp_fn.7]   30673 153.365000 13.250993
    CalcHourglassControlForElems(~~*, double) [clone ._omp_fn.6]   29055 145.275000 12.552003
    gomp_team_barrier_wait_end                                     21708 108.540000  9.378038
    IntegrateStressForElems(Domai~~ int, int) [clone ._omp_fn.4]   18856  94.280000  8.145950
    CalcKinematicsForElems(Domain~~uble, int) [clone ._omp_fn.0]   11793  58.965000  5.094675
    CalcMonotonicQGradientsForElems(Domain&) [clone ._omp_fn.14]    6489  32.445000  2.803302
    brk                                                             5848  29.240000  2.526385
    psm2_mq_ipeek2                                                  4067  20.335000  1.756978
    ...
