.. Caliper documentation master file, created by
   sphinx-quickstart on Wed Aug 12 16:55:34 2015.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Caliper: A Performance Analysis Toolbox in a Library
================================================================

Caliper is a program instrumentation and performance measurement
framework. It is a performance-analysis toolbox in a
library, allowing one to bake performance analysis capabilities
directly into applications and activate them at runtime.
Caliper is primarily aimed at HPC applications, but works for
any C/C++/Fortran program on Unix or Linux.

Caliper's data-collection mechanisms and source-code annotation API
support a variety of performance-engineering use cases, e.g.,
performance profiling, tracing, monitoring, and auto-tuning.

Features include:

* Low-overhead source-code annotation API
* Configuration API to control performance measurements from
  within an application
* Flexible key:value data model -- capture application-specific
  features for performance analysis
* Fully threadsafe implementation, support for parallel programming
  models like MPI
* Trace and profile recording with flexible online and offline
  data aggregation
* Connection to third-party tools, e.g., NVidia Nsight/NVProf and
  Intel(R) VTune(tm)
* Measurement and profiling functionality, such as timers, PAPI
  hardware counters, and Linux perf_events
* Memory-allocation annotations: associate performance measurements
  with named memory regions

Caliper is available for download on `GitHub
<https://github.com/LLNL/Caliper>`_.



.. toctree::
   :maxdepth: 2

   CaliperBasics

The reference documentation below covers Caliper user APIs and specific usage
aspects in detail.

.. toctree::
   :maxdepth: 1

   build
   AnnotationAPI
   ConfigManagerAPI
   FortranSupport
   ThirdPartyTools
   BuiltinConfigurations
   configuration
   workflow
   services
   calql
   OutputFormats
   tools
   DeveloperGuide

Index
--------------------------------

* :ref:`genindex`
