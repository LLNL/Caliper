Caliper: Context Annotation Library (for Performance)
==========================================

[![Build Status](https://travis-ci.org/LLNL/Caliper.svg)](https://travis-ci.org/LLNL/Caliper)

Caliper is a generic context annotation system. It serves as an
abstraction layer between applications and performance tools,
providing a clean and easy-to-use interface for application developers
to express application semantics in a tool-agnostic way.
These annotations can then be used for a variety of performance
engineering use cases (e.g., performance monitoring,
profiling, tracing, and auto-tuning). 

Released under a BSD license, `LLNL-CODE-678900`. 
See `LICENSE` file for details.


Documentation
------------------------------------------

Extensive documentation is available here:
https://llnl.github.io/Caliper/

Usage examples of the C++ and C annotation interfaces are provided in 
`test/cali-basic.cpp` and `test/cali-basic-c.c`, respectively.

See the "Getting started" section below for a brief tutorial.

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

See the "Build and install" section in the documentation for further
information.

Getting started
------------------------------------------

Caliper provides annotation APIs for marking source-code regions or
exporting arbitrary data in the form of key:value attributes.

Caliper can then be configured to take "snapshots" of the provided
data at specified events. Optionally, measurement data,
e.g. timestamps, can be added to the snapshots. Source-code
annotations, measurement data providers, and snapshot configurations
can be flexibly combined to support a wide range of performance
analysis or monitoring use cases. 


### Build and link annotated programs

To use Caliper, add annotation statements to your program and link it
against the Caliper library. Programs must be linked with the Caliper
runtime (libcaliper.so).

    CALIPER_LIBS = -L$(CALIPER_DIR)/lib -lcaliper


### Source-code annotation

Caliper provides source-code annotation APIs for C, C++, and Fortran.

The following example marks "initialization" and "loop" phases in a
C++ code, and exports the main loop's current iteration counter.

```C++
#include <Annotation.h>

int main(int argc, char* argv[])
{
    // Mark begin of "initialization" phase
    cali::Annotation
        init_ann = cali::Annotation("initialization").begin();
    // perform initialization tasks
    int count = 4;
    // Mark end of "initialization" phase
    init_ann.end();

    if (count > 0) {
        // Mark begin of "loop" phase. The scope guard will
        // automatically end it at the end of the C++ scope
        cali::Annotation::Guard 
            g_loop( cali::Annotation("loop").begin() );

        double t = 0.0, delta_t = 1e-6;

        // Create "iteration" attribute to export the iteration count
        cali::Annotation iteration_ann("iteration");
        
        for (int i = 0; i < count; ++i) {
            // Export current iteration count under "iteration"
            iteration_ann.set(i);

            // A Caliper snapshot taken at this point will contain
            // { "loop", "iteration"=<i> }

            // perform computation
            t += delta_t;
        }

        // Clear the "iteration" attribute (otherwise, snapshots taken
        // after the loop will still contain the last "iteration" value)
        iteration_ann.end();
    }
}
```

### Run the program

Caliper will now be able to take and process snapshots
or access the information provided by the source-code annotations at
runtime. The source-code annotations also provide hooks to enable
various runtime actions, such as triggering snapshots or writing
traces.

By default, the annotation commands perform no actions other than
updating the blackboard. However, we can connect a Caliper-enabled
third-party tool to the program, or enable built-in Caliper "service"
modules to take measurements and collect data.

As an example, Caliper's built-in `trace` configuration profiles
trigger and write snapshots whenever any or specific attributes are
updated, generating a snapshot trace. A configuration profile can be
selected with the `CALI_CONFIG_PROFILE` environment variable:

    $ CALI_CONFIG_PROFILE=thread-trace ./cali-basic
    == CALIPER: Registered pthread service
    == CALIPER: Registered recorder service
    == CALIPER: Registered timestamp service
    == CALIPER: Initialized
    == CALIPER: Wrote 36 records.
    == CALIPER: Finished

With this configuration, Caliper will write a take a snapshot for each
attribute update performed by the annotation commands, calculate the
time spent in each of the annotated phases, and write the results in
form of a snapshot trace to a `.cali` file in the current working
directory.

### Analyze Data

Use the `cali-query` tool to filter, aggregate, or print the recorded
snapshots. For example, the following command will show us the time spent
in the "initialization" phase, in the entire "loop" phase, and in each
iteration of the example program: 

    $ ls *.cali
    160219-095419_5623_LQfNQTNgpqdM.cali
    $ cali-query --table \
          --print-attributes=iteration:loop:initialization:time.inclusive.duration \
          160219-095419_5623_LQfNQTNgpqdM.cali
    initialization=true,time.inclusive.duration=202
    iteration=0,loop=true,time.inclusive.duration=51
    iteration=1,loop=true,time.inclusive.duration=24
    iteration=2,loop=true,time.inclusive.duration=17
    iteration=3,loop=true,time.inclusive.duration=24
    loop=true,time.inclusive.duration=211

### Where to go from here?

Caliper allows a great amount of flexibility and control in utilizing
source-code annotations. The "Usage examples" section in the
documentation demonstrates some of the many ways to use Caliper.  Much
of Caliper's functionality is implemented by built-in "services",
which can be enabled or disabled as needed. Refer to the "Caliper
services" section to learn about functionality they provide.  Finally,
the "Annotation API" section in the documentation provides reference
documentation for Caliper's C, C++, and Fortran annotation APIs.
