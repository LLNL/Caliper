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
An easy way to do this is to use one of Caliper's built-in measurement 
configurations. For example, the `runtime-report` config prints out the time 
spent in the annotated regions. You can activate built-in measurement 
configurations with the :doc:envvar:`CALI_CONFIG` environment variable.
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

Notes on multi-threading
................................

Some care must be taken when annotating multi-threaded programs. Regions are 
*either* visible only on the thread that creates them, *or* shared by all 
threads. The visibility scope (``thread`` or ``process``) can be set with the 
``CALI_CALIPER_ATTRIBUTE_DEFAULT_SCOPE`` configuration variable. It is set to
``thread`` by default.

A common practice is to mark code regions only on the master thread, outside 
of multi-threaded regions:

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
in the example above, the reported metrics (such as time per region) are only
for the master thread. However, sampling-based configurations can take 
measurements on all threads, regardless of region markers. In that case, the 
``process`` visibility scope allows us to associate these measurements with 
the "parallel" and "main" regions on any thread.

In contrast, in the example below, we enter and exit the "parallel" region on 
every thread, and the metrics reported by `runtime-report` therefore cover all
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


ConfigManager API
--------------------------------



.. _more-on-configurations:

More on configurations
--------------------------------

Loop profiling
--------------------------------

Recording program metadata
--------------------------------

NVidia NVProf/NSight and VTune
--------------------------------

.. _cxx-example:

C++ example program
--------------------------------

.. literalinclude:: ../../examples/apps/cxx-example.cpp
   :language: C++
