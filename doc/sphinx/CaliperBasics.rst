Caliper Basics
================================

Unlike many traditional performance tools, Caliper is a library that lives as
part of the application. This way, performance analysis can always be enabled
without requiring special tool-specific workflows. Profiling can even be
always-on, say to write a basic performance report at the end of each run.

The downside to integrated performance tools is the manual effort of adding the
library to the application. Similar to any other library, Caliper has an API
that must be called by the application. In addition, Caliper works alongside
another library, Adiak, to collect application run metadata. This is extremely
useful for comparing large numbers of runs with analysis frameworks like
`Thicket <https://github.com/LLNL/thicket>`_ and TreeScape.

There are several steps to integrate Caliper into an application:

* Add Caliper (and, optionally, Adiak) to the application's build system.
* Add Caliper annotations to interesting regions of the code. These
  annotations put labels over code regions that take a relevant amount of
  time to execute.
* Decide on a policy of what level of performance analysis should be on
  by default, if any.
* Optionally, add infrastructure to allow users to specify what level
  of performance analysis they want. This could be in the form of a
  command-line argument or input deck argument. Alternatively, users can
  use the ``CALI_CONFIG`` environment variable to control profiling.
* Optionally, initialize Adiak and pass name/value pairs that describe
  metadata about this run, such as a problem size or set of enabled
  physics packages. We will cover metadata collection in the
  :ref:`recording_metadata` section.
  of performance analysis they want.

Most of these steps are relatively easy and involve only a few lines of code.
Adding annotations throughout the code can be significant effort, though it
can also be done in stages with only a minimal level at the beginning and further
annotations refining the performance analysis data.

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

Caliper provides the `caliper` CMake target, which can be used to add
a dependency on Caliper in CMake: ::

  find_package(caliper)
  add_executable(MyExample MyExample.cpp)
  target_link_libraries(MyExample PRIVATE caliper)

The Caliper CMake package file lives in `share/cmake/caliper` inside the
Caliper installation directory. If the Caliper installation directory is not
already in the CMake package search path you can point the CMake executable to
it with `-Dcaliper_DIR`: ::

  cmake -Dcaliper_DIR=<caliper-installation-dir>/share/cmake/caliper ..

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
functions. In C++, you can use :c:macro:`CALI_CXX_MARK_SCOPE` to mark a
region that automatically closes when exiting the current C++ scope.

.. code-block:: c++

    #include <caliper/cali.h>

    // ...
    CALI_MARK_BEGIN("my region");
    // ...
    CALI_MARK_END("my region");

    {
        CALI_CXX_MARK_SCOPE("my scope region");
        // scope region automatically closes when leaving the current C++ scope
    }

Regions can be nested, but they must be stacked properly, i.e. the name
in an `end region` call must match the current innermost open region.
For more details, including C and Fortran examples, refer to the annotation
API reference: :doc:`AnnotationAPI`.

With the source-code annotations in place, we can run performance measurements.
By default, Caliper does not record data - we have to activate performance
profiling at runtime.
An easy way to do this is to use one of Caliper's built-in measurement
recipes. For example, the `runtime-report` recipe prints out the time
spent in the annotated regions. You can activate built-in measurement
recipes with the :ref:`configmgr_api` or with the
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

Like most built-in recipes, the runtime-report config works for MPI and
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

Caliper provides many more measurement recipes that make use of region
annotations. For example, `runtime-profile` writes a .cali file with region
times for processing with `Hatchet <https://github.com/LLNL/hatchet>`_ or
Caliper's `cali-query` tool. See :ref:`more-on-configurations` below to learn
more about different configuration recipes and their options.

Region levels and filtering
................................

Caliper supports region levels to allow collection of profiling data at different
granularities. The default regions have region level 0 (the finest level).
Caliper also provides "phase" region macros to mark larger program phases, such
as a physics package in a multi-physics code. Phase regions have region level 4.

.. code-block:: c++

    #include <caliper/cali.h>

    // ...
    CALI_MARK_PHASE_BEGIN("hydrodynamics");
    // ...
    CALI_MARK_PHASE_END("hydrodynamics");

Use the `level` option for the built-in configurations to select the desired
measurement granularity level. For example `runtime-report,level=phase`
will only measure regions that have at least "phase" level.

One can also include/exclude regions or entire branches by name. See
:doc:`RegionFiltering` to learn more.

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
recipes (see :ref:`more-on-configurations` below). The ConfigManager
interprets a short configuration string that can be hard-coded in the program
or provided by the user in some form, e.g. as a command-line parameter or
in the program's configuration file.

To use the ConfigManager API, create a
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
            std::cerr << "Caliper error: " << mgr.error_msg() << std::endl;

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
:envvar:`CALI_CONFIG` environment variable is a list of
*configs* (like `runtime-report`) and *parameters*.
Multiple configs can be specified, separated by comma.

Parameters can configure output options or enable additional functionality.
They can be specified as a list of key-value pairs in parentheses
after the config name, e.g. `runtime-report(output=report.txt,io.bytes)`. For
boolean parameters, only the key needs to be added to enable it; for example,
`io.bytes` is equal to `io.bytes=true`. You can also add parameters outside
of parentheses; these apply to all configs.

In the example below, we enable the `mem.highwatermark` option in
`runtime-report`. This adds the "Allocated MB" column that shows the maximum
amount of memory that was allocated in each region:

.. code-block:: sh

    $ ./examples/apps/cxx-example -P runtime-report,mem.highwatermark
    Path       Min time/rank Max time/rank Avg time/rank Time %   Allocated MB
    main            0.000179      0.000179      0.000179 2.054637     0.000047
      mainloop      0.000082      0.000082      0.000082 0.941230     0.000016
        foo         0.000778      0.000778      0.000778 8.930211     0.000016
      init          0.000020      0.000020      0.000020 0.229568     0.000000

You can use ``cali-query --help=configs`` to list all available recipes and their
parameters. You can also query parameters for a specific recipe, e.g.
``cali-query --help=runtime-report``.

Some available performance measurement recipes include:

runtime-report
   Print a time profile for annotated regions.

loop-report
   Print summary and time-series information for loops.

mpi-report
   Print time spent in MPI functions.

sample-report
   Print time spent in functions using call-path sampling.
   See :doc:`SampleProfiling`.

cuda-activity-report
   Record and print CUDA activities (kernel executions, memcopies, etc.)
   See :doc:`GPUProfiling`.

cuda-activity-profile
   Record CUDA activities and a write profile file (json or .cali)
   See :doc:`GPUProfiling`.

openmp-report
   Record and print OpenMP performance metrics (loops, barriers, etc.).
   Requires OMPT support. See :doc:`OpenMP`.

event-trace
   Record a trace of region enter/exit events in .cali format.
   See :doc:`EventTracing`.

runtime-profile
   Record a region time profile for processing with hatchet or cali-query.

sample-profile
   Record a sampling profile for processing with hatchet or cali-query.
   See :doc:`SampleProfiling`.

spot
   Record a time profile for Spot (internal LLNL visualization tool) or
   `Thicket <https://github.com/LLNL/thicket>`_, a Python performance analysis
   framework for comparing performance profiles from many program configurations.

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

With loop annotations in place, we can use the `loop.stats` option to print
the minimum, maximum, and average time per loop iteration:

.. code-block:: sh

    $ ./examples/apps/cxx-example -P runtime-report,loop.stats 5000
    Path       Time (E) Time (I) Time % (E) Time % (I) Iterations Time/iter (min) Time/iter (avg) Time/iter (max)
    main       0.000070 8.010493   0.000870  99.995709
      init     0.000004 0.000004   0.000047   0.000047
      mainloop 0.172615 8.010420   2.154765  99.994792       5000        0.000110        0.001591        0.003317
        foo    7.837805 7.837805  97.840027  97.840027

More detailed loop timing information is available with the loop-report
recipe:

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

See :doc:`BuiltinConfigurations` or run ``cali-query --help=loop-report``
to learn about all loop-report options. Loop profiling is also available
with other configs, notably the `spot` config producing output for the Spot
performance visualization web framework.

.. _recording_metadata:

Recording program metadata
--------------------------------

Caliper is often used for performance comparison studies involving large
collections of runs - for example, automatic performance regression testing,
scaling studies, or comparing different program configurations. To that end,
Caliper can store metadata name-value pairs to describe and distinguish
performance profiles from different runs.

There are several complementary ways to record metadata name-value pairs
in a Caliper profile:

* Using the `Adiak <https://github.com/LLNL/Adiak>`_ library
* Using Caliper metadata attributes
* Providing metadata name-value pairs in the Caliper config string
* Reading metadata name-value pairs from a JSON file

We recommend using Adiak, which provides a user-friendly API as well as
built-in functionality to record common information provided by the OS
and runtime systems.
Generally, we recommend recording any variables that are relevant to
distinguishing and understanding the run generating the performance
profile, such as:

* The version / build date / git hash of the code
* Build information, like the compiler and optimization level used
* Versions of important libraries
* Application configuration and input parameters, such as problem size and
  decomposition settings, enabled physics packages, algorithms used, etc.
* Machine and execution information, e.g. OS version, machine name,
  date/time of the run
* Information about the kind/purpose of the run, such as a test or experiment
  name
* Application-generated figure-of-merit metrics

Using Adiak
................................

Caliper works together with `Adiak <https://github.com/LLNL/Adiak>`_, a C/C++
library to record program metadata. Detailed documentation for Adiak is
available `here <https://software.llnl.gov/Adiak>`_. This section covers basic
use of Adiak for recording run metadata in an application.

At its core, Adiak is an in-memory key-value store. To use Adiak, an application
first initializes Adiak and then registers name-value pairs with Adiak's data
collection API. By default, Adiak makes deep copies of all passed-in values: it
is not intended for storing large datasets, but for collecting descriptive
run metadata.

Caliper automatically imports all name-value pairs collected with Adiak as run
metadata in .cali or .json output files. They can also be printed in text-based
recipes like runtime-report with the ``print.metadata`` option, e.g.
``CALI_CONFIG=runtime-report,print.metadata``.

Like Caliper, Adiak provides a CMake package file. The Adiak CMake package
contains the ``adiak::adiak`` target, which should be linked to the target code: ::

    find_package(adiak)
    target_link_libraries(basic_example adiak::adiak)

The Adiak CMake package file lives in ``lib/cmake/adiak`` inside the Adiak
installation directory. Set ``-Dadiak_DIR`` to point CMake to the Adiak package: ::
  
    $ cmake -Dadiak_DIR=/path-to-adiak/lib/cmake/adiak

C++ source files using Adiak should include ``adiak.hpp``. C sources should
include ``adiak.h``.

Adiak should be initialized with ``adiak::init(void*)`` (in C++) or
``adiak_init(void*)`` (in C). The initialization function takes a pointer to
an MPI communicator, or ``nullptr`` in a non-MPI program. Initializing
Adiak with an MPI communicator allows it to collect certain MPI-specific
information, such as the MPI job size and MPI library version.

At exit, Adiak should be finalized with ``adiak::fini()`` in C++ or ``adiak_fini()``
in C. Calling fini is important for collecting end-of-process data (such as job
runtime) and flushing data. If used in an MPI-enabled adiak, then fini should be
called before MPI_Finalize():

.. code-block:: c++

    #include <adiak.hpp>

    int main(int argc, char* argv[])
    {
        MPI_Init(&argc, &argv);
        MPI_Comm adk_comm = MPI_COMM_WORLD;
        // Pass a pointer to an MPI communicator or NULL to skip MPI support
        adiak::init(&adk_comm);
        // ...
        adiak::fini(); // Call adiak::fini() before MPI_Finalize
        MPI_Finalize();
    }

Adiak has two types of functions:

* An implicit interface to collect system-level values stored under standardized names
* An explicit interface to collect application-level data under user-defined names

The implicit interface has a set of functions like ``adiak::launchdate()`` or
``adiak::user()`` to collect system-provided information like the launch date or
user name. A complete list of functions is available in the Adiak
documentation. We recommend using the convenient collect_all shorthand, which
collects all available implicit Adiak variables:

.. code-block:: c++

    bool adiak::collect_all(); // C++ version to collect all implicit Adiak variables
    int adiak_collect_all(); // C version

Program-specific data can be recorded with the ``adiak::value`` template in C++:

.. code-block:: c++

    template<typename T>
    bool value(std::string name, T value, int category = adiak_general, std::string subcategory = "")

It takes two required and two optional parameters:

* The name under which the value is stored
* The value. Adiak accepts many C++ datatypes, including compound types like STL vectors.
* (Optional) a category. Typical run metadata should use the default `adiak_general` category.
* (Optional) a user-defined subcategory. Typically left empty.

Adiak's internal type system supports many common datatypes, including
integrals (integers and floating-point values), strings, UNIX time objects,
as well as compound types like lists and tuples. There are also specialized
types such as "path" and "version" for strings that represent file paths or
program versions, respectively. The ``adiak::value`` template automatically
derives an appropriate Adiak datatype from the passed-in value. There are also
converters like ``adiak::path`` and ``adiak::version`` to convert strings to the
specialized "path" and "version" types. Here are a few examples:

.. code-block:: c++

    adiak::value("maxtemperature", 70.2);
    adiak::value("compiler", adiak::version("gcc@13.3.0"));
    adiak::value("input_file", adiak::path("/home/user/in.dat"));

    std::array<int, 3> dims = { 8, 8, 16 };
    adiak::value("dimensions", dims);

C programs should use the `adiak_namevalue` function, which uses a printf-style
type descriptor to describe the desired datatype:

.. code-block:: c++

    int adiak_namevalue(const char *name, int category, const char *subcategory, const char *typestr, ...);

Supported data types include integers (`%d`, `%u`), strings (`%s`), specialized
strings like program versions (`%v`), and even compound types like arrays and
structs. See `adiak_namevalue <https://software.llnl.gov/Adiak/ApplicationAPI.html#_CPPv415adiak_namevaluePKciPKcPKcz>`_
in the Adiak documentation for more details. Examples:

.. code-block:: c++

    adiak_namevalue("numrecords", adiak_general, NULL, "%d", 10);
    adiak_namevalue("buildcompiler", adiak_general, NULL, "%v", "gcc@4.7.3");

    double gridvalues[] = { 5.4, 18.1, 24.0, 92.8 };
    adiak_namevalue("gridvals", adiak_general, NULL, "[%f]", gridvalues, 4);

    struct { int pos; const char *val; } letters[3] = { {1, 'a'}, {2, 'b'}, {3, 'c'} };
    adiak_namevalue("alphabet", adiak_general, NULL, "[(%u, %s)]", letters, 3, 2);

Using the Caliper global value API
..................................

Internally, Caliper stores metadata values as attributes with the
`CALI_ATTR_GLOBAL` property. These can be created and set conveniently with
the `cali_set_global_string|int|uint|double_byname` function family in C
and C++:

.. code-block:: c++

    cali_set_global_double_byname("maxtemperature", 70.2);
    cali_set_global_string_byname("version", "0.99");

Unlike Adiak the Caliper functions do not support complex or custom
datatypes.

Providing metadata in config strings
....................................

You can also pass in metadata name-value pairs in a `CALI_CONFIG`
or ConfigManager config string using the `metadata` keyword like so: ::

    $ CALI_CONFIG="runtime-report,print.metadata,metadata(foo=fooval,bar=barval)" ./examples/apps/cxx-example
    caliper.config        :
    iterations            : 4
    cali.caliper.version  : 2.12.0-dev
    opts:print.metadata   : true
    opts:output.append    : true
    opts:order_as_visited : true
    foo                   : fooval
    bar                   : barval
    cali.channel          : runtime-report
    Path       Time (E) Time (I) Time % (E) Time % (I)
    main       0.000014 0.000547   2.118374  85.205938
    ...

Values passed in through the `metadata` keyword are always recorded as strings.

Finally, Caliper can read metadata name-value pairs from a JSON file at
runtime. To do so, specify a file name and (optionally) a list of dictionary
keys to read in a `metadata` entry in a Caliper config string with the
special `file` and `keys` arguments:

.. code-block:: sh

    CALI_CONFIG="spot,metadata(file=data.json,keys=\"experiment,expected_result\")"

The JSON file should contain a dictionary: ::

    {
     "experiment": "metadata_test", "expected_result": 42, "extra": { "val": 4242 }
    }

If a list of keys was provided, Caliper will only read the specified dictionary
entries from the file, otherwise it will read all entries. The
dictionaries can be nested. In this case Caliper will record a name-value pair
for each sub-entry where the name is the path of keys separated with ".". For
example, with the file above Caliper would create a `extra.val=4242` name-value
pair.

There can be multiple `metadata` entries in a Caliper config string, each
with a list of name-value pairs or a file specification.

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
