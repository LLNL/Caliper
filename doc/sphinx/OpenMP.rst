OpenMP profiling
================================

Caliper can profile OpenMP constructs with the help of the OpenMP tools
interface (OMPT). This requires a compiler with OMPT support, e.g. clang 9+.
Build Caliper with ``-DWITH_OMPT=On`` to enable it.

When OMPT support is enabled, Caliper provides the `openmp-report` built-in
config, as well as the `openmp.times` and `openmp.threads` options for configs
like `runtime-report` and `hatchet-region-profile`. With manual configurations,
you can use the :ref:`ompt-service` service.

OpenMP profiling with openmp-report
------------------------------------

The `openmp-report` config measures and prints the time spent in OpenMP
constructs on each thread::

    $ CALI_CONFIG=openmp-report ./caliper-openmp-example
    Path   #Threads Time (thread) Time (total) Work %    Barrier % Time (work) Time (barrier)
    main                 0.005122     0.027660 85.969388 14.030612
      work        4      0.005110     0.027572 85.969388 14.030612    0.011121       0.001815

This shows example output for a program like the one in section
:ref:`omp-instrumentation`. For Caliper regions with active OpenMP parallel
regions inside them (like "work" in the example), the report prints the
total CPU time across all threads spent doing OpenMP work ("Time (work)") and
stuck in OpenMP barriers ("Time (barrier)"). It also computes "Work %" and
"Barrier %" metrics, which indicate the relative amount of time spent in work
and barrier regions, respectively. These two metrics are *inclusive*, so we
see the overall OpenMP efficiency for the entire program in the "main" region.
The definition of the metrics is as follows:

Path
    The Caliper region hierarchy.

#Threads
    Maximum number of OpenMP threads in the region.

Time (thread)
    Time in seconds spent on the main thread (i.e., wall-clock time).

Time (total)
    Sum of CPU time in seconds across all threads.

Work %
    Percent of CPU time spent in OpenMP workshare regions vs. overall time in
    OpenMP. This is an inclusive metric (e.g., aggregated over all child
    regions).

Barrier %
    Percent of CPU time spent in barriers vs. overall time in OpenMP. This is
    also an inclusive metric.

Time (work)
    CPU time spent in OpenMP workshare regions (e.g., ``#pragma omp for`` loops).

Time (barrier)
    CPU time spent in OpenMP barriers (both implicit and explicit barriers).

Profiling by thread
.................................

With the "show_threads" option, we can see the times spent on each thread::

    $ CALI_CONFIG=openmp-report,show_threads=1,show_regions=true ./caliper-openmp-example
    Path   #Threads Time (thread) Time (total) Thread Work %     Barrier % Time (work) Time (barrier)
    main
     |-                  0.000197     0.014312
     |-                               0.002747      3 100.000000
     |-                               0.003086      1 100.000000
     |-                               0.002856      2 100.000000
     |-                  0.005075     0.005075      0  62.539218 37.460782
      work
       |-                0.000180     0.014228
       |-         4                   0.002747      3 100.000000              0.002688
       |-         4                   0.003086      1 100.000000              0.003018
       |-         4                   0.002856      2 100.000000              0.002800
       |-         4      0.005075     0.005075      0  62.539218 37.460782    0.002990       0.001791

We now see the additional "Thread" column with the OpenMP thread ID, and
"Time (total)" shows the CPU time in each OpenMP thread per Caliper region.
The rows without a "Thread" entry refer to time spent outside OpenMP parallel
regions.

Thread summary view
.................................

With "show_regions=false", we can disable the grouping by Caliper region and
just show a performance summary across OpenMP threads::

    $ CALI_CONFIG=openmp-report,show_threads=true,show_regions=false
    #Threads Time (thread) Time (total) Thread Work %     Barrier % Time (work) Time (barrier)
                  0.000175     0.016727
           4                   0.002760      3 100.000000              0.002722
           4                   0.002958      1 100.000000              0.002901
           4                   0.002768      2 100.000000              0.002699
           4      0.004913     0.004913      0  62.682091 37.317909    0.002926       0.00174

.. _omp-instrumentation:

OpenMP options for other configs
--------------------------------

Caliper also provides the OpenMP options for other profiling configs such as
 `hatchet-region-profile`. These are:

openmp.times
    Measure and return CPU time spent in OpenMP workshare and barrier regions.

openmp.efficiency
    Compute the "Work %" and "Barrier %" metrics as in the openmp-report config
    shown above.

openmp.threads
    Group by thread (i.e., record metrics for each OpenMP thread separately),
    as with the "show_threads" option for `openmp-report` shown above.

Instrumentation
--------------------------------

When instrumenting the code, make sure to use "process"-scope annotations
to mark code regions outside of OpenMP parallel regions, *or* put thread-scope
annotations onto every OpenMP thread. Mixing both approaches is possible but
not recommended, since it produces separate region hierarchies for the process
and thread scopes. See :ref:`notes_on_threading` for more information.

Use the ``CALI_CALIPER_ATTRIBUTE_DEFAULT_SCOPE`` config flag to define if
Caliper regions should use process or thread scope. They use thread scope
by default. The following example sets the attribute default scope to
"process" so that the "main" and "work" Caliper regions are visible on all
OpenMP threads inside the parallel region:

.. code-block:: c++

    #include <caliper/cali.h>

    int main()
    {
        cali_config_set("CALI_CALIPER_ATTRIBUTE_DEFAULT_SCOPE", "process");

        CALI_MARK_BEGIN("main");
        CALI_MARK_BEGIN("work");

    #pragma omp parallel
        {
            // ...
        }

        CALI_MARK_END("work");
        CALI_MARK_END("main");
    }

Enabling OMPT
-------------------------------

Caliper enables the OpenMP tools interface automatically when the `ompt`
service is active. In some cases this can fail: this is often the case when
the OpenMP runtime is initialized before Caliper. In this case, set the
:envvar:`CALI_USE_OMPT` environment variable to "1" or "true" to enable
OpenMP support manually.
