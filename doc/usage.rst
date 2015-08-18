Usage Overview
================================

To use Caliper, add annotation statements to your programs and/or
libraries, link your program with the Caliper library, and select an
appropriate run-time configuration.

Source-code annotation
--------------------------------

Here is a simple source-code annotation example:

.. code-block:: c++

    #include <Annotation.h>

    int main(int argc, char* argv[])
    {
        cali::Annotation phase_ann("phase");

        phase_ann.begin("main");                   // Context is "phase=main"

        phase_ann.begin("init");                   // Context is "phase=main/init"
        int count = 4;
        phase_ann.end();                           // Context is "phase=main"

        if (count > 0) {
            cali::Annotation::AutoScope
                loop_s( phase_ann.begin("loop") ); // Context is "phase=main/loop"

            cali::Annotation
                iteration_ann("iteration", CALI_ATTR_ASVALUE);

            for (int i = 0; i < count; ++i) {
                iteration_ann.set(i);              // Context is "phase=main/loop,iteration=i"
            }

            iteration_ann.end();                   // Context is "phase=main/loop"
        }                                          // Context is "phase=main"

        phase_ann.end();                           // Context is ""
    }

A `cali::Annotation` object creates and stores an annotation attribute.
An annotation attribute should have a unique name.
The example above creates two annotation attributes, "phase" and "iteration".

The *Caliper context* is the set of all active attribute/value pairs.
Use the `begin()`, `end()` and `set()` methods of an annotation object
to set, unset and modify context values.  The `begin()` and `set()`
methods are overloaded for common data types (strings, integers, and
floating point).

- `cali::Annotation::begin(value)` puts `value` into the context for
  the given annotation attribute.  The value is appended to the current
  values of the attribute, which allows you to create hierarchies (as in
  the "phase" annotation in the example).
- `cali::Annotation::set(value)` sets the value of the annotation
  attribute to `value`.  It overwrites the current value of the
  annotation attribute on the same hierarchy level.
- `cali::Annotation::end()` removes the innermost value of the given
  annotation attribute from the context. It is the user's
  responsibility to nest `begin()`/`set()` and `end()` calls
  correctly.

Linking annotated programs
--------------------------------

To build a program with Caliper annotations, link it with the Caliper
runtime (libcaliper) and infrastructure library (libcaliper-common).

.. code-block:: sh

    CALIPER_LIBS = -L$(CALIPER_DIR)/lib -lcaliper -lcaliper-common

Depending on the configuration, you may also need to add the
`libunwind`, `papi`, and `pthread` libraries to the link line.

Running annotated programs
--------------------------------

An annotated program generates a Caliper context at runtime. Caliper
service modules or external tools can trigger snapshots of the current
program context combined with measurement data. When snapshots are
taken, what is included in them, and how they are processed depends on
the Caliper runtime configuration. It is thus important to configure
the runtime correctly when running a Caliper-instrumented program.

Much of Caliper's functionality is provided by optional service
modules. By default, Caliper does not enable any optional services
(and therefore doesn't do much). Use the `CALI_SERVICES_ENABLE`
variable to select services. For example, enable the `event`,
`recorder` and `timestamp` services to create a simple time-series
trace for the example program above:

.. code-block:: sh
                
    $ export CALI_SERVICES_ENABLE=event:recorder:timestamp

You can achieve the same effect by selecting the `serial-trace`
built-in profile:

.. code-block:: sh

    $ export CALI_CONFIG_PROFILE=serial-trace

Then run the program:

.. code-block:: sh

    $ ./cali-basic
    == CALIPER: Registered recorder service
    == CALIPER: Registered timestamp service
    == CALIPER: Initialized
    == CALIPER: Wrote 36 records.
    == CALIPER: Finished

The recorder service will write the time-series trace to a `.cali`
file in the current working directory. Use the `cali-query` tool to
filter, aggregate, or print traces.

Runtime configuration
--------------------------------

The Caliper library is configured through configuration variables. You
can provide configuration variables as environment variables or in a
text file `caliper.config` in the current working directory.

Here is a list of commonly used variables:

- `CALI_CONFIG_PROFILE`=(serial-trace|thread-trace|mpi-trace|...)
  Select a built-in or self-defined configuration
  profile. Configuration profiles allow you to select a pre-defined
  configuration. The `serial-trace`, `thread-trace` and `mpi-trace`
  built-in profiles create event-triggered context traces of serial,
  multi-threaded, or MPI programs, respectively. See the documentation
  on how to create your own configuration profiles.
- `CALI_LOG_VERBOSITY=(0|1|2)` Verbosity level for log messages. Set to 1 for
  informational output, 2 for more verbose output, or 0 to disable
  output except for critical error messages. Default 1.
- `CALI_SERVICES_ENABLE=(service1:service2:...)` List of Caliper service
  modules to enable, separated by `:`. Default: not set, no service
  modules enabled.

Analyze and export data
--------------------------------

Use the `cali-query` tool to filter, aggregate, or print traces:

.. code-block:: sh

    $ ls *.cali
    150407-092621_96557_aGxI5Q9Zh2uU.cali
    $ cali-query -e 150407-092621_96557_aGxI5Q9Zh2uU.cali
    time.duration=57
    phase=main,time.duration=33
    phase=init/main,time.duration=7
    phase=main,time.duration=6
    phase=loop/main,time.duration=9
    iteration=0,phase=loop/main,time.duration=9
    iteration=1,phase=loop/main,time.duration=4
    iteration=2,phase=loop/main,time.duration=4
    iteration=3,phase=loop/main,time.duration=4
    phase=loop/main,time.duration=5
    phase=main,time.duration=6

