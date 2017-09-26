.. Caliper documentation master file, created by
   sphinx-quickstart on Wed Aug 12 16:55:34 2015.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Caliper: Context Annotation Library for Performance Analysis
================================================================

Caliper is a program instrumentation and performance measurement
framework. It provides data collection mechanisms and a source-code
annotation API for a variety of performance engineering use cases,
e.g., performance profiling, tracing, monitoring, and
auto-tuning. Caliper primarily targets parallel HPC applications,
but it can be used in any C/C++ or Fortran project.
Features include:

* Low-overhead source-code annotation API for C, C++ and Fortran
* Creation of lightweight performance reports, or detailed profiles
  or traces 
* Flexible key:value data model: capture application-specific
  features for performance analysis
* Fully threadsafe implementation and support for parallel programming
  models such as MPI
* Synchronous and asynchronous data collection (sampling)
* Runtime-configurable performance data recording toolbox: combine 
  independent building blocks for custom analysis tasks

Caliper is available for download on
`github <https://github.com/LLNL/Caliper>`_.

Contents
--------------------------------
.. toctree::
   :maxdepth: 1

   build
   usage
   concepts
   AnnotationAPI
   link
   configuration
   services
   tools
   calql
   serviceapi

Index
--------------------------------

* :ref:`genindex`

