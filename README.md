Caliper: Context Annotation Library (for Performance)
==========================================

by David Boehme, boehme3@llnl.gov

Caliper is a generic context annotation system. It gives programmers
the ability to provide arbitrary program context information to 
(performance) tools at runtime.

Documentation
------------------------------------------

Extensive documentation can be found in the `doc/` directory.

A usage example of the C++ annotation interface is provided in 
`test/cali-basic.cpp`

See the "Getting started" section below for a brief tutorial.

License
------------------------------------------

See LICENSE file in this directory.

Building and installing
------------------------------------------

Building and installing Caliper requires cmake and a current C++11-compatible
Compiler. Unpack the source distribution and proceed as follows:

     cd <path to caliper root directory>
     mkdir build && cd build
     cmake -DCMAKE_INSTALL_PREFIX=<path to install location> \ 
         -DCMAKE_C_COMPILER=<path to c-compiler> \
         -DCMAKE_CXX_COMPILER=<path to c++-compiler> \
         ..
     make 
     make install

The OMPT header file and libunwind are required to build the OMPT (OpenMP tools
interface) and callpath modules, respectively. Both modules are optional.

### Building on BG/Q

When building on a BlueGene/Q system, the libraries must be cross-compiled to
work correctly on the compute nodes. Use the provided toolchain file to build
with clang, like so:

     cd <path to caliper root directory>
     mkdir build && cd build
     cmake -DCMAKE_INSTALL_PREFIX=<path to install location> \ 
         -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/bgq-dynamic.toolchain \
         -DCMAKE_CXX_FLAGS=-stdlib=libc++ \
         ..
     make 
     make install

When processing the created cali files, make sure to use a version of
`cali-query` complied for the frontend node. 

Getting started
------------------------------------------

To use Caliper, add annotation statements to your program and link it against
the Caliper library.

### Source-code annotation

Here is a simple source-code annotation example:

```C++
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
```

A `cali::Annotation` object creates and stores an annotation attribute. 
An annotation attribute should have a unique name. 
The example above creates two annotation attributes, "phase" and "iteration".

The _Caliper context_ is the set of all active attribute/value pairs.
Use the `begin()`, `end()` and `set()` methods of an annotation object
to set, unset and modify context values.  The `begin()` and `set()`
methods are overloaded for common data types (strings, integers, and
floating point).

* `cali::Annotation::begin(value)` puts `value` into the context for
the given annotation attribute.  The value is appended to the current
values of the attribute, which allows you to create hierarchies (as in
the "phase" annotation in the example).

* `cali::Annotation::set(value)` sets the value of the annotation
attribute to `value`.  It overwrites the current value of the
annotation attribute on the same hierarchy level.

* `cali::Annotation::end()` removes the innermost value of the given
  annotation attribute from the context. It is the user's
  responsibility to nest `begin()`/`set()` and `end()` calls
  correctly.

### Build and link annotated programs

To build a program with Caliper annotations, link it with the Caliper
runtime (libcaliper) and infrastructure library (libcaliper-common).

    CALIPER_LIBS = -L$(CALIPER_DIR)/lib -lcaliper -lcaliper-common

Depending on the configuration, you may also need to add the 
`libunwind`, `papi`, and `pthread` libraries to the link line.

### Run the program

An annotated program generates a Caliper context at runtime. Caliper
service modules or external tools can trigger snapshots of the current
program context combined with measurement data. When snapshots are
taken, what is included in them, and how they are processed depends on
the Caliper runtime configuration. It is thus important to configure
the runtime correctly when running a Caliper-instrumented program.
For common use cases, Caliper provides built-in configuration profiles
as starting points. For example, the `serial-trace` profile will 
record a time-stamped event trace. You can select a profile with the 
`CALI_CONFIG_PROFILE` environment variable:

    $ export CALI_CONFIG_PROFILE=serial-trace

Then run the program:

    $ ./cali-basic
    == CALIPER: Registered recorder service
    == CALIPER: Registered timestamp service
    == CALIPER: Initialized
    == CALIPER: Wrote 36 records.
    == CALIPER: Finished

With this profile, Caliper will write a time-series trace to a `.cali`
file in the current working directory. Use the `cali-query` tool to
filter, aggregate, or print traces:

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

### Runtime configuration

The Caliper library is configured through configuration variables. You
can provide configuration variables as environment variables or in a
text file `caliper.config` in the current working directory.

Here is a list of commonly used variables:

* `CALI_CONFIG_PROFILE`=(serial-trace|thread-trace|mpi-trace|...)
  Select a built-in or self-defined configuration
  profile. Configuration profiles allow you to select a pre-defined
  configuration. The `serial-trace`, `thread-trace` and `mpi-trace`
  built-in profiles create event-triggered context traces of serial,
  multi-threaded, or MPI programs, respectively. See the documentation
  on how to create your own configuration profiles.

* `CALI_LOG_VERBOSITY=(0|1|2)` Verbosity level for log messages. Set to 1 for
  informational output, 2 for more verbose output, or 0 to disable
  output except for critical error messages. Default 1.

* `CALI_SERVICES_ENABLE=(service1:service2:...)` List of Caliper service 
  modules to enable, separated by `:`. Default: not set, no service modules 
  enabled. See below for a list of Caliper services.

### List of Caliper services

Caliper comes with a number of optional modules (*services*) that
provide measurement data or processing and data recording
capabilities. The flexible combination and configuration of these
services allows you to quickly assemble recording solutions for a wide
range of usage scenarios.

Many of the services provide additional configuration options. Refer to 
the service documentation to learn more.

You can enable the services required for your measurement with the
`CALI_SERVICES_ENABLE` configuration variable

The following services are available:

* `callpath` Add a call path to the caliper context
* `event` The event trigger service. Trigger a 
  measurement snapshot when attributes are updated.
* `debug` Print annotation and measurement events.
  Useful to debug source-code annotations.
* `mpi` Record MPI operations and the MPI rank.
* `ompt` The OpenMP tools interface service.
  Connects to the OpenMP tools interface to retrieve OpenMP status
  information.
  Requires an ompt-enabled OpenMP runtime.
* `pthread` Manages thread environments for any pthread-based
  multi-threading runtime system.
  A thread environment manager such as the `pthread` service creates
  separate per-thread contexts in a multi-threaded program.
* `papi` Records PAPI hardware counters.
* `recorder` Writes data to disk
* `trace` Creates a trace of snapshots
* `timestamp` The timestamp service adds a time offset, timestamp,
  or duration to context records.

### MPI programs

Each process in an MPI program (or any other distributed-memory
programming model) produces an independent Caliper dataset. Therefore,
the recorder service writes one Caliper file per process. Users can
merge / aggregate the separate datasets in post-processing. 

Caliper provides an MPI service module to record MPI information
via the PMPI interface. To enable the MPI service, the 
`libcaliper-mpiwrap` wrapper library needs to be linked to the
program, AND the mpi service needs to be activated by adding
`mpi` to the list of services with the `CALI_SERVICES_ENABLE` 
environment variable.
