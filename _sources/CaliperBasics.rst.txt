Caliper Basics
================================

Caliper is a library to integrate performance profiling capabilities
into applications. To use Caliper, developers mark code regions of
interest using Caliper's annotation API. Applications can then enable
performance profiling at runtime with Caliper's configuration API.
Alternatively, you can configure Caliper through environment variables
or config files.

This tutorial covers basic Caliper usage, including source-code
annotations and using Caliper's built-in performance measurement
configurations.
Most of the examples shown here use the ``cxx-example`` program in the
Caliper Git repository. There is a complete listing at the end of
this tutorial: :ref:`cxx-example`.

Build and install
--------------------------------

You can install Caliper with `Spack <https://github.com/spack/spack>`_, or
clone it directly from Github:

.. code-block:: sh

    $ git clone https://github.com/LLNL/Caliper.git

Caliper uses CMake and C++11. To build it, run cmake:

.. code-block:: sh

    $ mkdir build && cd build
    $ cmake ..
    $ make && make install

There are many build flags to enable optional features, such as `-DWITH_MPI`
for MPI support. See :doc:`build` for details.

Caliper installs files into the ``lib64``, ``include``, and ``bin`` directories
under `CMAKE_INSTALL_PREFIX`. To use Caliper, link ``libcaliper`` to the
target program:

.. code-block:: sh

    $ g++ -o app app.o -L<path to caliper installation>/lib64 -lcaliper

Region profiling
--------------------------------

Caliper's source-code annotation API allows you to mark source-code regions
of interest in your program. Much of Caliper's functionality depends on these
region annotations.

Caliper provides macros and functions for C, C++, and Fortran to mark
functions, loops, or sections of source-code. For example, use
:c:macro:`CALI_CXX_MARK_FUNCTION` to mark a function in C++:

.. code-block:: c++

    #include <caliper/cali.h>

    void foo()
    {
        CALI_CXX_MARK_FUNCTION;
        // ...
    }

You can mark arbitrary code regions with the :c:macro:`CALI_MARK_BEGIN`
and :c:macro:`CALI_MARK_END` macros or the corresponding
:cpp:func:`cali_begin_region()` and :cpp:func:`cali_end_region()`
functions:

.. code-block:: c++

    #include <caliper/cali.h>

    // ...
    CALI_MARK_BEGIN("my region");
    // ...
    CALI_MARK_END("my region");

You can have as many regions as you like. Regions can be nested, but they
must be stacked properly, i.e. the name in an `end region` call must match
the current innermost open region.
For more details, including C and Fortran examples, refer to the annotation
API reference: :doc:`AnnotationAPI`.

With the source-code annotations in place, we can run performance measurements.
By default, Caliper does not record data - we have to activate performance
profiling at runtime.
An easy way to do this is to use one of Caliper's built-in measurement
configurations. For example, the `runtime-report` config prints out the time
spent in the annotated regions. You can activate built-in measurement
configurations with the :ref:`configmgr_api` or with the
:envvar:`CALI_CONFIG` environment variable.
Let's try this on Caliper's cxx-example program:

.. code-block:: sh

    $ cd Caliper/build
    $ make cxx-example
    $ CALI_CONFIG=runtime-report ./examples/apps/cxx-example
    Path       Min time/rank Max time/rank Avg time/rank Time %
    main            0.000119      0.000119      0.000119  7.079120
      mainloop      0.000067      0.000067      0.000067  3.985723
        foo         0.000646      0.000646      0.000646 38.429506
      init          0.000017      0.000017      0.000017  1.011303

Like most built-in configurations, the runtime-report config works for MPI and
non-MPI programs. By default, it reports the minimum, maximum, and average
exclusive time (seconds) spent in each marked code region across MPI ranks
(the three values are identical in non-MPI programs). Exclusive time is the
time spent in a region without the time spent in its children.

You can customize the report with additional options. Some options enable
additional Caliper functionality, such as profiling MPI and CUDA functions in
addition to the user-defined regions, or additional metrics like memory usage.
Another example is the `calc.inclusive` option, which prints inclusive instead
of exclusive region times:

.. code-block:: sh

    $ CALI_CONFIG=runtime-report,calc.inclusive ./examples/apps/cxx-example
    Path       Min time/rank Max time/rank Avg time/rank Time %
    main            0.000658      0.000658      0.000658 55.247691
      mainloop      0.000637      0.000637      0.000637 53.484467
        foo         0.000624      0.000624      0.000624 52.392947
      init          0.000003      0.000003      0.000003  0.251889

Caliper provides many more performance measurement configurations in addition
to `runtime-report` that make use of region annotations. For example,
`hatchet-region-profile` writes a json file with region times for processing
with `Hatchet <https://github.com/LLNL/hatchet>`_. See
:ref:`more-on-configurations` below to learn more about different
configurations and their options.


.. _notes_on_threading:

Notes on multi-threading
................................

Some care must be taken when annotating multi-threaded programs. Regions are
*either* visible only on the thread that creates them, *or* shared by all
threads. You can set the visibility scope (``thread`` or ``process``) with the
``CALI_CALIPER_ATTRIBUTE_DEFAULT_SCOPE`` configuration variable. It is set to
``thread`` by default.

A common practice is to mark code regions only on the master thread, outside
of multi-threaded regions. In this case, it is useful to set the visibility
scope to ``process``:

.. code-block:: c++

    #include <caliper/cali.h>

    int main()
    {
        cali_config_set("CALI_CALIPER_ATTRIBUTE_DEFAULT_SCOPE", "process");

        CALI_MARK_BEGIN("main");
        CALI_MARK_BEGIN("parallel");

    #pragma omp parallel
        {
            // ...
        }

        CALI_MARK_END("parallel");
        CALI_MARK_END("main");
    }

The annotation placement inside or outside of threads also affects performance
measurements: in event-based measurement configurations (e.g., `runtime-report`),
measurements are taken when entering and exiting annotated regions. Therefore,
in the example above, the reported performance metrics (such as time per region)
are only for the master thread. However, sampling-based configurations like
callpath-sample-report can take measurements on all threads, regardless of
region markers. The ``process`` visibility scope then allows us to associate
these measurements with the "parallel" and "main" regions on any thread.

In contrast, in the example below, we enter and exit the "parallel" region on
every thread, and metrics reported by `runtime-report` therefore cover all
threads. However, the "main" region is only visible on the master thread.

.. code-block:: c++

    #include <caliper/cali.h>

    int main()
    {
        CALI_MARK_BEGIN("main");

    #pragma omp parallel
        {
            CALI_MARK_BEGIN("parallel");
            // ...
            CALI_MARK_END("parallel");
        }

        CALI_MARK_END("main");
    }


.. _configmgr_api:

ConfigManager API
--------------------------------

A distinctive Caliper feature is the ability to enable performance
measurements programmatically with the ConfigManager API. For example, we often
let users activate performance measurements with a command-line argument.

The ConfigManager API provides access to Caliper's built-in measurement
configurations (see :ref:`more-on-configurations` below). The ConfigManager
interprets a short configuration string that can be hard-coded in the program
or provided by the user in some form, e.g. as a command-line parameter or
in the program's configuration file.

To access and control the built-in configurations, create a
:cpp:class:`cali::ConfigManager` object. Add a configuration string with
``add()``, start the requested configuration channels with ``start()``,
and trigger output with ``flush()``. In MPI programs, the ``flush()`` method
must be called before ``MPI_Finalize``:

.. code-block:: c++

    #include <caliper/cali.h>
    #include <caliper/cali-manager.h>

    #include <mpi.h>

    #include <iostream>

    int main(int argc, char* argv[])
    {
        MPI_Init(&argc, &argv);

        cali::ConfigManager mgr;
        mgr.add(argv[1]);

        // Check for configuration errors
        if (mgr.error())
            std::cerr "Caliper error: " << mgr.error_msg() << std::endl;

        // Start configured performance measurements, if any
        mgr.start();

        // ...

        // Flush output before finalizing MPI
        mgr.flush();

        MPI_Finalize();
    }

The :ref:`cxx-example` uses the ConfigManager API to let users specify a
Caliper configuration with the `-P` command-line argument, e.g.
``-P runtime-report``:

.. code-block:: sh

    $ ./examples/apps/cxx-example -P runtime-report
    Path       Min time/rank Max time/rank Avg time/rank Time %
    main            0.000129      0.000129      0.000129  5.952930
      mainloop      0.000080      0.000080      0.000080  3.691740
        foo         0.000719      0.000719      0.000719 33.179511
      init          0.000021      0.000021      0.000021  0.969082

See :doc:`ConfigManagerAPI` for the complete API documentation. The
``c-example`` and ``fortran-example`` example programs in the Caliper source
show how to use the ConfigManager API from C and Fortran, respectively. See
:ref:`more-on-configurations` below for the configuration string syntax.

Notes:

* Multiple ConfigManager objects can co-exist in a program.
* The ConfigManager API can be used in combination with
  the :envvar:`CALI_CONFIG` environment variable or manual
  configurations.

.. _more-on-configurations:

More on configurations
--------------------------------

A configuration string for the ConfigManager API or the
:envvar:`CALI_CONFIG` environment variable is a comma-separated list of
*configs* and *parameters*.

A *config* is the name of one of Caliper's built-in measurement configurations,
e.g. `runtime-report`. Multiple configs can be specified, separated by comma.

Most configs have optional parameters, e.g. `output` to name an output file.
Parameters can be specified as a list of key-value pairs in parentheses after
the config name, e.g. `runtime-report(output=report.txt,io.bytes)`. For
boolean parameters, only the key needs to be added to enable it; for example,
`io.bytes` is equal to `io.bytes=true`. You can also add parameters outside
of parentheses; these apply to all configs.

Many optional parameters enable additional Caliper functionality. For example,
the `profile.mpi` option enables MPI function profiling, the `io.bytes` option
reports I/O bytes written and read, and the `mem.highwatermark` option reports
the memory high-watermark. In the example below, the `mem.highwatermark`
option for `runtime-report` adds the "Allocated MB" column that shows the
maximum amount of memory that was allocated in each region:

.. code-block:: sh

    $ ./examples/apps/cxx-example -P runtime-report,mem.highwatermark
    Path       Min time/rank Max time/rank Avg time/rank Time %   Allocated MB
    main            0.000179      0.000179      0.000179 2.054637     0.000047
      mainloop      0.000082      0.000082      0.000082 0.941230     0.000016
        foo         0.000778      0.000778      0.000778 8.930211     0.000016
      init          0.000020      0.000020      0.000020 0.229568     0.000000

You can use the cali-query and mpi-caliquery programs to list available
configs and their parameters (note that cali-query does not list MPI-dependent
options and configs). For example, ``mpi-caliquery --help=configs`` lists all
configs and their options. You can also query parameters for a specific config,
e.g. ``mpi-caliquery --help=runtime-report``.

Some available performance measurement configs include:

runtime-report
   Print a time profile for annotated regions.

loop-report
   Print summary and time-series information for loops.

mpi-report
   Print time spent in MPI functions.

callpath-sample-report
   Print time spent in functions using call-path sampling.

cuda-activity-report
   Record and print CUDA activities (kernel executions, memcopies, etc.)

cuda-activity-profile
   Record CUDA activities and a write profile file (json or .cali)

openmp-report
   Record and print OpenMP performance metrics (loops, barriers, etc.).
   Requires OMPT support.

event-trace
   Record a trace of region enter/exit events in .cali format.

hatchet-region-profile
   Record a region time profile for processing with hatchet or cali-query.

hatchet-sample-profile
   Record a sampling profile for processing with hatchet or cali-query.

spot
   Record a time profile for the Spot web visualization framework.

We discuss some of these configurations below. For a complete reference of the
configuration string syntax and available configs and parameters, see
:doc:`BuiltinConfigurations`.

You can also create entirely custom measurement configurations by selecting and
configuring Caliper services manually. See :doc:`configuration` to learn more.

Loop profiling
--------------------------------

Loop profiling allows analysis of performance over time in iterative programs.
To use loop profiling, annotate loops and loop iterations with Caliper's loop
annotation macros:

.. code-block:: c++

    CALI_CXX_MARK_LOOP_BEGIN(mainloop_id, "mainloop");

    for (int i = 0; i < N; ++i) {
        CALI_CXX_MARK_LOOP_ITERATION(mainloop_id, i);
        // ...
    }

    CALI_CXX_MARK_LOOP_END(mainloop_id);

The ``CALI_CXX_MARK_LOOP_BEGIN`` macro gets a unique identifier (`mainloop_id`
in the example) that is referenced in the subsequent iteration marker and loop
end macros, as well as a user-defined name for the loop (here: "mainloop").

Like other region annotations, loop and iteration annotations are meant for
high-level regions, not small, frequently executed loops inside kernels.
We recommend to only annotate top-level loops, such as the main timestepping
loop in a simulation code.
With the loop annotations in place, we can use the loop-report config to print
loop performance information:

.. code-block:: sh

    $ ./examples/apps/cxx-example -P loop-report 5000
    Loop summary:
    ------------

    Loop     Iterations Time (s) Iter/s (min) Iter/s (max) Iter/s (avg)
    mainloop       5000 7.012186   389.323701  2406.931322   713.178697

    Iteration summary (mainloop):
    -----------------

    Block Iterations Time (s) Iter/s
        0       1204 0.500222 2406.931322
     1204        536 0.500921 1070.029007
     1740        429 0.500715  856.774812
     2169        365 0.501464  727.868800
     2534        324 0.500800  646.964856
     2858        295 0.501248  588.531027
     3153        272 0.500612  543.334958
     3425        254 0.501714  506.264525
     3679        249 0.501720  496.292753
     3928        237 0.500152  473.855948
     4165        223 0.501182  444.948143
     4388        215 0.501665  428.572852
     4603        203 0.501471  404.809052
     4806        194 0.498300  389.323701

Here, we run the cxx-example program with 5000 loop iterations. The loop-report
config prints an overall performance summary ("Loop summary") and a time-series
summary ("Iteration summary") for each instrumented top-level loop. The
iteration summary shows loop performance grouped by iteration blocks.
By default, the report shows at most 20 iteration blocks. The block size adapts
to cover the entire loop.

Caliper's loop profiling typically does not measure every single iteration.
By default, we take measurements at iteration boundaries after at least 0.5
seconds have passed since the previous measurement. This keeps the runtime
overhead of loop profiling very low.
The example program adds an increasing delay in each loop iteration. In the
output above, we see this in the decreasing amount of iterations in each
block and decreasing performance ("Iter/s"). Because of the time-based
measurements, the time for each iteration block is the same.

We can configure the measurement mode and output. The `iteration_interval`
option switches to an iteration-based instead of a time-based measurement
interval. For example, we can take measurements every 500 loop iterations:

.. code-block:: sh

    $ ./examples/apps/cxx-example -P loop-report,iteration_interval=500 5000
    Iteration summary (mainloop):
    -----------------

    Block Iterations Time (s) Iter/s
                   0 0.000032    0.000000
        0        500 0.110812 4512.146699
      500        500 0.244453 2045.382957
     1000        500 0.378453 1321.168018
     1500        500 0.532856  938.339814
     2000        500 0.660435  757.076775
     2500        500 0.785368  636.644223
     3000        500 0.911957  548.271465
     3500        500 1.034089  483.517376
     4000        500 1.159672  431.156396
     4500        500 1.285956  388.815792

Now, the iterations per block remain at 500, whereas the time for
each block increases. The iterations per second ("Iter/s") column provides
a useful performance metric independent of the measurement mode.

The report aggregates the data into a maximum number of iteration blocks
(20 by default) to avoid visual clutter in programs with long-running loops.
We can change this number with the `timeseries.maxrows` option. For example,
we can choose a maximum of 3 blocks:

.. code-block:: sh

    $ ./examples/apps/cxx-example -P loop-report,iteration_interval=500,timeseries.maxrows=3 5000
    Iteration summary (mainloop):
    -----------------

    Block Iterations Time (s) Iter/s
                   0 0.000034    0.000000
        0       2000 1.294308 1545.227257
     1666       1500 2.359132  635.827075
     3332       1500 3.484032  430.535655

Setting ``timeseries.maxrows=0`` disables the block limit and outputs all
measured blocks. Thus, the configuration

.. code-block::

    loop-report,iteration_interval=1,timeseries.maxrows=0

shows performance data for every single loop iteration.

We can enable other performance metrics in the loop report, such as the memory
high-water mark:

.. code-block:: sh

    $ ./examples/apps/cxx-example -P loop-report,timeseries.maxrows=4,mem.highwatermark 5000
    Loop summary:
    ------------

    Loop     Iterations Time (s) Iter/s (min) Iter/s (max) Iter/s (avg) Allocated MB
    mainloop       5000 7.138860   371.436531  2408.400822   684.238039     0.000016

    Iteration summary (mainloop):
    -----------------

    Block Iterations Time (s) Iter/s      Allocated MB
        0       1745 1.001062 1743.148776     0.000016
     1250        788 1.000955  787.248178     0.000016
     2500       1382 2.502447  552.259448     0.000016
     3750       1085 2.634396  411.859113     0.000016

See :doc:`BuiltinConfigurations` or run ``mpi-caliquery --help=loop-report``
to learn about all loop-report options. Loop profiling is also available
with other configs, notably the `spot` config producing output for the Spot
performance visualization web framework.

Recording program metadata
--------------------------------

Caliper can record and store program metadata, such as the system and program
configuration, in its output files. This is useful for studies that compare
data from multiple runs, such as scaling studies.

The Caliper annotation API provides the
``cali_set_global_(double|int|string|uint)_byname()`` family of functions to
save global attributes in the form of key-value pairs:

.. code-block:: c++

    cali_set_global_int_byname("iterations", iterations);
    cali_set_global_string_byname("caliper.config", configstr.c_str());

Most machine-readable output formats, e.g. the hatchet JSON format written by the
hatchet-region-profile config, include this data:

.. code-block:: sh

    $ ./examples/apps/cxx-example -P hatchet-region-profile,output=stdout
    {
    "data": [
        ...
    ],
    ...
    "caliper.config": "hatchet-region-profile,output=stdout",
    "iterations": "4",
    "cali.caliper.version": "2.5.0-dev",
    "cali.channel": "hatchet-region-profile"
    }

Note how the "iterations" and "caliper.config" attributes are stored as
top-level attributes in the JSON output. Caliper adds some built-in metadata
attributes as well, such as the Caliper version ("cali.caliper.version").

An even better way to record metadata is the `Adiak <https://github.com/LLNL/Adiak>`_
library. Adiak makes metadata attributes accessible to multiple tools, and
provides built-in functionality to record common information such as user
name, executable name, command-line arguments, etc. The example below uses
Adiak to record the "iterations" and "caliper.config" attributes as shown
above, and also the user name, launch date, and MPI job size:

.. code-block:: c++

    adiak::user();
    adiak::launchdate();
    adiak::jobsize();

    adiak::value("iterations", iterations);
    adiak::value("caliper.config", configstr.c_str());

Most Caliper configs automatically import metadata attributes set through
Adiak (Adiak support must be enabled in the Caliper build configuration). The
spot config for the Spot web visualization framework requires that metadata
attributes are recorded through Adiak.

Third-party tool support (NVidia NSight, Intel VTune)
-----------------------------------------------------

Caliper provides bindings to export Caliper-annotated source code regions to
third-party tools. Currently, Nvidia's NVTX API for the NVProf/NSight
profilers and Intel's ITT API for Intel VTune Amplifier are supported.

To use the NVTX forwarding, activate the "nvtx" Caliper config when
recording data with nvprof or ncu, either with the :doc:envvar:`CALI_CONFIG`
environment variable, or the ConfigManager API. Be sure to enable NVTX support
in NSight Compute.

To use the vtune bindings, run the target application in VTune with
the `vtune` service enabled. To do so in the VTune GUI, do the
following:

* In the "Analysis Target" tab, go to the "User-defined environment
  variables" section, and add an entry setting "CALI_SERVICES_ENABLE"
  to "vtune"
* In the "Analysis Type" tab, check the "Analyze user tasks, events,
  and counters" checkbox.

Caliper-annotated regions will then be visible as "tasks" in the VTune
analysis views.

.. _cxx-example:

C++ example program
--------------------------------

.. literalinclude:: ../../examples/apps/cxx-example.cpp
   :language: C++
