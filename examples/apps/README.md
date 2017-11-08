Caliper example applications
==========================================

This directory contains a set of small example programs to demonstrate
the use of the Caliper APIs.

* `cali-basic-annotations.cpp` and `cali-basic-annotations-c.c`
  demonstrate the use of Caliper annotation macros in C++ and C,
  respectively.
* `cali-functional.cpp` demonstrates a template-based
  inline function annotation interface (experimental feature).
* `cali-memtracking.cpp` and `cali-memtracking-macros.cpp` demonstrate
  Caliper APIs to mark and name allocated memory regions for
  subsequent analysis with Caliper.
* `cali-print-snapshot.c` demonstrates the C interface to pull and
  evaluate Caliper snapshots at runtime.
