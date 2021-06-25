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
* Recording program metadata for analyzing collections of runs
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

Caliper Basics
-------------------------------

This section covers basic Caliper usage, including source-code annotations and
using Caliper's built-in performance measurement configurations:

.. toctree::
   :maxdepth: 2

   CaliperBasics

Guides
-------------------------------

This section lists how-to articles for various use cases.

.. toctree::
   :maxdepth: 1

   CUDA
   OpenMP

Reference documentation
-------------------------------

The reference documentation below covers Caliper user APIs and specific usage
aspects in detail:

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

Materials
--------------------------------

Tutorial slides: :download:`2021 ECP Annual Meeting Tutorial slide deck <2021_ECP_Caliper_Tutorial.pdf>`.

About
--------------------------------

Caliper was created by `David Boehme <https://github.com/daboehme>`_.

A complete `list of contributors <https://github.com/LLNL/Caliper/graphs/contributors>`_ is available on GitHub.

Major contributors include:

* `Alfredo Gimenez <https://github.com/alfredo-gimenez>`_ (libpfm support, memory allocation tracking)
* `David Poliakoff <https://github.com/DavidPoliakoff>`_ (GOTCHA support)

To reference Caliper in a publication, please cite the following
`paper <http://ieeexplore.ieee.org/abstract/document/7877125/>`_:

* David Boehme, Todd Gamblin, David Beckingsale, Peer-Timo Bremer,
  Alfredo Gimenez, Matthew LeGendre, Olga Pearce, and Martin
  Schulz.
  **Caliper: Performance Introspection for HPC Software Stacks**.
  In *Supercomputing 2016 (SC16)*, Salt Lake City, Utah,
  November 13-18, 2016. LLNL-CONF-699263.

Caliper is released under a BSD 3-clause license.
See `LICENSE <https://github.com/LLNL/Caliper/blob/master/LICENSE>`_
for details.

LLNL Software release ID LLNL-CODE-678900.

Index
--------------------------------

* :ref:`genindex`
