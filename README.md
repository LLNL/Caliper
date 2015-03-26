Caliper: Context Annotation Library (for Performance)
==========================================

by David Boehme, boehme3@llnl.gov

Caliper is a generic context annotation system. It gives programmers
the ability to provide arbitrary program context information to 
(performance) tools at runtime.

Documentation
------------------------------------------

For now, the code and API is evidently self-documenting.

A usage example of the C++ annotation interface is provided in 
`test/cali-basic.cpp`

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


Getting started
------------------------------------------

To use Caliper, add annotation statements to your program and link it against
the Caliper library.
