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

Much of Caliper's functionality is provided by optional service
modules. By default, Caliper does not enable any optional services
(and therefore doesn't do much). Use the `CALI_SERVICES_ENABLE`
variable to select services. For example, enable the `event`,
`recorder` and `timestamp` services to create a simple time-series
trace for the example program above:

    $ export CALI_SERVICES_ENABLE=event:recorder:timestamp

You can achieve the same effect by selecting the `serial-trace`
built-in profile:

    $ export CALI_CONFIG_PROFILE=serial-trace

Then run the program:

    $ ./cali-basic
    == CALIPER: Registered recorder service
    == CALIPER: Registered timestamp service
    == CALIPER: Initialized
    == CALIPER: Wrote 36 records.
    == CALIPER: Finished

The recorder service will write the time-series trace to a `.cali`
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

* `CALI_CALIPER_AUTOMERGE=(true|false)` Automatically merge attributes
  into a common context tree. This will usually reduce the size of
  context records, but may reduce performance and increase metadata
  size. Default `true`.

* `CALI_LOG_LOGFILE=(stdout|stderr|filename)` File name for Caliper information messages. May
  be set to `stdout` or `stderr` to print the standard output or error
  streams, respectively. Default `stderr`.

* `CALI_LOG_VERBOSITY=(0|1|2)` Verbosity level for log messages. Set to 1 for
  informational output, 2 for more verbose output, or 0 to disable
  output except for critical error messages. Default 1.

* `CALI_SERVICES_ENABLE=(service1:service2:...)` List of Caliper service modules to enable,
  separated by `:`. Default: not set, no service modules enabled. See
  below for a list of Caliper services.

### List of Caliper services

This is a list of Caliper service modules and their configuration
options.
Note that each service has to be enabled via `CALI_SERVICES_ENABLE` to
take effect.

* `callpath` Add a call path to the caliper context
  * `CALI_CALLPATH_USE_NAME=(true|false)` Use region names for call path.
    Incurs higher overhead.
    Default: false.
  * `CALI_CALLPATH_USE_ADDRESS=(true|false)` Use stack-frame
    addresses.
    Default: true.
  * `CALI_CALLPATH_SKIP_FRAMES=(number of frames)` Skip this many
    frames from the call path.
    This is used to remove call paths within the Caliper library from
    the context.
    Default: 10.

* `event` The event trigger service. This service will trigger a 
  context/measurement snapshot when attributes are updated.
  * `CALI_EVENT_TRIGGER`=(attr1:attr2:...) List of attributes where
    snapshots are triggered. If empty, snapshots are triggered on
    every attribute update.
  
* `debug` Print annotation and measurement events.
  Useful to debug source-code annotations.

* `mpi` Record MPI operations and the MPI rank.
  Note that `libcaliper-mpiwrap` library has to be linked with the 
  application in addition to the regular Caliper libraries to
  obtain MPI information. See "MPI programs" below.
  * `CALI_MPI_WHITELIST`=(MPI_Fn_1:MPI_Fn_2:...) List of MPI functions 
    to instrument.
    If set, only whitelisted functions will be instrumented.
  * `CALI_MPI_BLACKLIST`=(MPI_Fn_1:MPI_Fn_2:...) List of MPI functions 
    that will be filtered. Note: if both whitelist and blacklist
    are set, only whitelisted functions will be instrumented, and
    the blacklist will be applied to the whitelisted
    functions.

* `ompt` The OpenMP tools interface service.
    Connects to the OpenMP tools interface to retrieve OpenMP status
    information.
    Requires an ompt-enabled OpenMP runtime.
  * `CALI_OMPT_ENVIRONMENT_MAPPING=(true|false)` Manage
    thread environment in the ompt module.
    Incurs higher overhead than the `pthread` service.
    Default: false.
  * `CALI_OMPT_CAPTURE_STATE=(true|false)` Add the OpenMP runtime
    state to context records.
    Default: true.

* `pthread` Manages thread environments for any pthread-based
  multi-threading runtime system.
  A thread environment manager such as the `pthread` service creates
  separate per-thread contexts in a multi-threaded program.

* `papi` Records PAPI hardware counters.
  * `CALI_PAPI_COUNTERS` List of PAPI counters to record, separated by 
    colon (`:`).

* `recorder` Writes context trace data.
  * `CALI_RECORDER_FILENAME=(stdout|stderr|filename)` File name for
    context trace. 
    May be set to `stdout` or `stderr` to print the standard output or
    error streams, respectively.
    Default: not set, auto-generates a unique file name.
  * `CALI_RECORDER_DIRECTORY=(directory name)` Directory to write
    context trace files to.
    The directory must exist, Caliper does not create it.
    Default: not set, uses current working directory.
  * `CALI_RECORDER_RECORD_BUFFER_SIZE=(number of records)` Initial number of records
    that can be stored in the in-memory record buffer.
    Default: 8000.
  * `CALI_RECORDER_DATA_BUFFER_SIZE=(number of data elements)` Initial number of
    data elements that can be stored in the in-memory record buffer.
    Default: 60000.
  * `CALI_RECORDER_BUFFER_CAN_GROW=(true|false)` Allow record and data
    buffers to grow if necessary.
    If false, buffer content will be flushed to disk when either buffer
    is full.
    Default: true.
    
* `timestamp` The timestamp service adds a time offset, timestamp,
  or duration to context records.
  Note that timestamps are _not_ synchronized between machines in a
  distributed-memory program.
  * `CALI_TIMER_DURATION=(true|false)` Include duration (in
    microsecond) of context epoch with context records.
    Default: true.
  * `CALI_TIMER_OFFSET=(true|false)` Include time offset (time
    since program start, in microseconds) with each context record.
    Default: false.
  * `CALI_TIMER_TIMESTAMP=(true|false)` Include absolute timestamp
    (time since UNIX epoch, in seconds) with each context record.

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
