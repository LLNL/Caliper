Caliper: Context Annotation Library (for Performance)
==========================================

by David Boehme, boehme3@llnl.gov

Caliper is a generic context annotation system. It gives programmers
the ability to provide arbitrary program context information to 
(performance) tools at runtime.

Documentation
------------------------------------------

A usage example of the C++ annotation interface is provided in 
`test/cali-basic.cpp`

See the "Getting started" section below for a brief tutorial.

License
------------------------------------------

TBD

Building and installing
------------------------------------------

Building and installing Caliper requires cmake and a current C++11-compatible
Compiler. Unpack the source distribution and proceed as follows:

     cd caliper/src/dir
     mkdir build && cd build
     cmake -DCMAKE_INSTALL_PREFIX=/path/to/install/location ..
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

```
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
Use the `begin()`, `end()` and `set()` methods to set, unset and modify context values.
The `begin()` and `set()` methods are overloaded for common data types (strings, integers, and floating point).

* `cali::Annotation::begin(value)` puts `value` into the context for the given annotation attribute.
The value is appended to the current values of the attribute, which allows you to create hierarchies (as in the "phase" annotation in the example).

* `cali::Annotation::set(value)` sets the value of the annotation attribute to `value`.
It overwrites the current value of the annotation attribute on the same hierarchy level.

* `cali::Annotation::end()` removes the innermost value of the given annotation attribute from the context. It is the user's responsibility to nest `begin()`/`set()` and `end()` calls correctly.

### Build and link annotated programs

To build a program with Caliper annotations, link it with the Caliper libraries.
At this point, the Caliper libraries are built statically.
Therefore, all Caliper modules must be explicitly linked to the annotated program.
The full list and order of the libraries is the following:

    CALIPER_LIBS = -L$(CALIPER_DIR)/lib -lcaliper -lcaliper-services \ -lcaliper-callpath -lcaliper-csv -lcaliper-debug -lcaliper-recorder -lcaliper-ompt -lcaliper-pthread -lcaliper-timestamp -lcaliper -lcaliper-common

Depending on the configuration, you might need to omit the OMPT (`-lcaliper-ompt`) or the callpath module (`-lcaliper-callpath`).

Also, because Caliper is written in C++11, all source files with Caliper annotations must be compiled in C++11 mode (typically by using the `-std=c++11` compiler flag).

### Run the program



### Runtime configuration


