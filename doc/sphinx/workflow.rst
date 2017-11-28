Architecture and Workflow
================================

Caliper's highly modular architecture allow for a wide range of use
cases. For example, it can be used as a library to access application
context information from third-party tools, or it can be configured as
a stand-alone performance data recorder.

To use Caliper as a performance profiler or trace recorder, we need to
set up a data recording pipeline by enabling appropriate services on
each pipeline stage. The following figure illustrates the recording
pipeline:

.. figure:: caliper-services-workflow.png
    :scale: 60
            
    Caliper components and the data recording pipeline. The orange boxes
    are core components of the Caliper runtime library, the blue boxes
    are services.

Caliper services can be used in any combination. However, to produce
any output, at least one component must be active in each stage of the
recording pipeline. The following sections discuss the pipeline stages
in detail.

Context Information and Event Hooks
--------------------------------

Context information is the key:value data provided by Caliper's
annotation API. For example, an application can mark a section of code
with the ``CALI_MARK_BEGIN`` and ``CALI_MARK_END`` macros, or export
any application-specific information that may be relevant for
performance analysis in key:value form with, e.g., ``cali_set_int``.

Context information collection is active for any target program that
contains Caliper annotations and does not need to be configured
explicitly. There are also services that provide additional
context information through the annotation API, for example ``mpi``
(marks MPI functions), ``ompt`` (marks OpenMP regions using the OpenMP
tools interface), or ``cupti`` (marks CUDA runtime and driver API
calls).

In addition to providing context information, the API calls also serve
as hooks for Caliper-internal callbacks. These can be used to trigger
snapshots (see below) or connect Caliper annotations to a third-party
instrumentation API.

Runtime Data Sources
--------------------------------

Runtime data sources provide the context and performance data that
Caliper records. The primary data source is the *blackboard*, which
contains all information provided by the annotation API. Context
information stays on the blackboard until it is explicitly cleared or
overwritten (generally, a *begin* annotation call will put context
information on the blackboard, and the corresponding *end* call will
clear it). Hence, the blackboard always contains the current
application context as defined by annotation API. The blackboard is
always active.

Aside from the blackboard, a number of services provide additional
performance or program context data (e.g., timer, PAPI hardware
counters, Linux perf events, and callpath). These are not active by
default and must be explicitly activated if needed.

Note that enabling runtime data sources only makes them accessible:
actual data collection happens through *snapshots*.

Snapshots
--------------------------------

Snapshots are Caliper's mechanism to make performance measurements and
collect data. Specifically, a snapshot collects data from all active
runtime data sources and creates a snapshot record. The snapshot
record thus contains the blackboard contents (with the program context
information at the moment the snapshot was taken), along with any data
provided by the active runtime data source services (such as
timestamps or hardware counter information).

Snapshots can be triggered explicitly through the Caliper API, or by a
snapshot trigger service. The two major snapshot trigger services in
Caliper are ``event`` and ``sampler`` (some other services, such as
``libpfm``, can also trigger snapshots). The ``event`` service
triggers a snapshot on each annotation *begin*, *set*, or *end*
event. This allows us to create event-based traces and profiles,
collecting exact statistics and performance measurements for annotated
code regions. The ``sampler`` service triggers snapshots periodically
with a configurable frequency. This asynchronous data collection mode
is less exact (we may miss some annotated code regions if they are
very short), but may incur less overhead. Moreover, sampling can
provide basic information about code regions that have not been
annotated, in particular in combination with call stack unwinding.
