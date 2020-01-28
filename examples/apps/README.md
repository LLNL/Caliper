Caliper example applications
==========================================

This directory contains a set of small example programs to demonstrate
the use of various Caliper APIs.

* The [cxx-example](cxx-example.cpp) program demonstrates Caliper's C++ 
  annotation macros and the `ConfigManager` API
* The [cali-regionprofile](cali-regionprofile.cpp) example demonstrates 
  the `RegionProfile` class to query Caliper region times within a C++ application
* The [cali-functional](cali-functional.cpp) example demonstrates a template-based
  inline function annotation interface (experimental feature).
* The [cali-memtracking](cali-memtracking.cpp) and 
  [cali-memtracking-macros](cali-memtracking-macros.cpp) examples demonstrate Caliper 
  APIs to mark and name allocated memory regions for subsequent analysis with Caliper.
* The [cali-print-snapshot](cali-print-snapshot.c) example demonstrates the C interface 
  to pull and process Caliper snapshots at runtime.
