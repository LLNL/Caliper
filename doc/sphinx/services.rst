Caliper Services
================================

Caliper comes with a number of optional modules (*services*) that
provide measurement data or processing and data recording
capabilities. The flexible combination and configuration of these
services allows you to quickly assemble recording solutions for a wide
range of usage scenarios.

You can enable the services required for your measurement with the
``CALI_SERVICES_ENABLE`` configuration variable, e.g.::

  export CALI_SERVICES_ENABLE=event,recorder,trace

to create event-triggered context traces for an application.

The following sections describe the available service modules and
their configuration.

.. _aggregate-service:

Aggregate
--------------------------------

The `aggregate` service accumulates aggregation attributes (e.g., time
durations) of snapshots with a similar `key`, creating a profile.

.. envvar:: CALI_AGGREGATE_KEY

   Colon-separated list of attributes that form the aggregation key
   (see below).

   Default: Empty (all attributes without the ``ASVALUE`` storage
   property are key attributes).

.. envvar:: CALI_AGGREGATE_ATTRIBUTES

   Colon-separated list of aggregation attributes. The `aggregate`
   service aggregates values of aggregation attributes from all input
   snapshots with similar aggregation keys. Note that only attributes
   with the ``ASVALUE`` storage property can be aggregation
   attributes.

   By default, the aggregation service determines aggregatable
   attributes automatically. With this configuration variable,
   the aggregation attributes can be set specifically (e.g., to
   select a subset). When set to `none`, the `aggregate` service
   will not aggregate any attributes, and only count the number of
   snapshots with similar keys.

   Default: Empty (determine aggregation attributes automatically).

Aggregation key
................................

The aggregation key defines for which attributes separate (aggregate)
snapshot records will be kept. That is, the aggregate service will
generate an aggregate snapshot record for each unique combination of
key values found in the input snapshots.  The values of the
aggregation attributes in the input snapshots will be accumulated and
appended to the aggregate snapshot.  The aggregate snapshot records
also include a `count` attribute that indicates how many
input snapshots with the given aggregation key were found. Attributes
that are neither aggregation attributes nor part of the aggregation
key will not appear in the aggregate snapshot records.

As an example, consider the following program:

.. code-block:: c++

    #include <caliper/Annotation.h>

    void foo(int c) {
        cali::Annotation::Guard
          g( cali::Annotation("function").begin("foo") );

        // ...
    }

    int main()
    {
      { // "A" loop
        cali::Annotation::Guard
          g( cali::Annotation("loop.id").begin("A") );

        for (int i = 0; i < 3; ++i) {
            cali::Annotation("iteration").set(i);

            foo(1);
            foo(2);
        }
      }

      { // "B" loop
        cali::Annotation::Guard
          g( cali::Annotation("loop.id").begin("B") );

        for (int i = 0; i < 4; ++i) {
            cali::Annotation("iteration").set(i);

            foo(1);
        }
      }
    }

Assuming snapshots are generated from the `function` annotation and
the aggregation key contains the `function`, `loop.id`, and
`iteration` attributes, the `aggregate` service will generate the
following aggregate snapshots : ::

    loop.id=A  iteration=0  function=foo  count=2
    loop.id=A  iteration=1  function=foo  count=2
    loop.id=A  iteration=2  function=foo  count=2
    loop.id=B  iteration=0  function=foo  count=1
    loop.id=B  iteration=1  function=foo  count=1
    loop.id=B  iteration=2  function=foo  count=1
    loop.id=B  iteration=3  function=foo  count=1

Removing the `iteration` attribute from the aggregation key will
collapse input snapshots with different iteration values into a
single aggregate snapshot: ::

    loop.id=A  function=foo  count=6
    loop.id=B  function=foo  count=4

Aggregation attributes
................................

The aggregate service accumulates values of aggregation attributes in
input snapshots with similar aggregation keys. Specifically, it
reports the minimum and maximum value, and computes the sum of the
aggregation attributes in the input snapshots. Aggregate snapshots
include `(min|max|sum)#attribute-name` attributes with the
minimum, maximum, and sum values for each aggregation attribute,
respectively.

Note that only attributes with the ``ASVALUE`` property can be
aggregation attributes.

Example
................................

The following configuration generates a time profile for the function
annotation separated by loop id. Note: when using
`time.inclusive.duration` as aggregation attribute, we strongly
recommend to include the `event.end` event attributes for all
annotations of interest in the aggregation key (e.g.,
`event.end#function` in the example), or use the default, empty key.

.. code-block:: sh

   $ CALI_SERVICES_ENABLE=aggregate:event:recorder:timestamp \
       CALI_EVENT_TRIGGER=function \
       CALI_AGGREGATE_KEY=event.end#function:loop.id \
         ./test/cali-basic-aggregate
   == CALIPER: Registered aggregation service
   == CALIPER: Registered event service
   == CALIPER: Registered recorder service
   == CALIPER: Registered timestamp service
   == CALIPER: Initialized
   == CALIPER: aggregate: flushed 4 snapshots.
   == CALIPER: Wrote 57 records.

The resulting file has the following contents: ::

   loop.id=A  event.end#function=foo  count=6
     min#time.inclusive.duration=25
     max#time.inclusive.duration=26
     sum#time.inclusice.duration=151
   loop.id=B  event.end#function=foo  count=4
     min#time.inclusive.duration=25
     max#time.inclusive.duration=26
     sum#time.inclusice.duration=102

.. _alloc-service:

Alloc
--------------------------------

The `alloc` service adds data tracking information to Caliper.
It records snapshots of allocation calls with their arguments and
return values, and resolves the containing allocations of any memory
addresses produced by other Caliper services, such as the `libpfm` 
service.
By default, it will only use data tracking information provided via
the Caliper data tracking API, but in conjunction with the
`sysalloc` service it records and/or tracks any allocations by
hooking system allocation calls.
This service may potentially incur significant amounts of overhead when 
recording/tracking frequent allocations/deallocations.

.. envvar:: CALI_ALLOC_TRACK_ALLOCATIONS

    Records snapshots when memory regions are being tracked or
    untracked, storing the given label in the `mem.alloc` or
    `mem.free` attribute, respectively. The snapshots also contain a
    unique ID for the allocation in the `alloc.uid` attribute, and the
    size of the allocated region in the `alloc.total_size` attribute.

    Default: true

.. envvar:: CALI_ALLOC_RESOLVE_ADDRESSES

    When set, snapshots with memory addresses produced by other
    services (e.g., Libpfm) will be appended with the allocations that
    contain them. The snapshots then contain
    `alloc.label#address_attribute`, `alloc.uid#address_attribute`,
    and `alloc.index#address_attribute` attributes with the memory
    region label, allocation ID, and array index for the memory
    address attributes found in the snapshot.

    Default: false

.. envvar:: CALI_ALLOC_RECORD_ACTIVE_MEM

    Records the amount of active allocated memory, in bytes, at each
    snapshot, in the `mem.active` attribute.

    Default: false

.. _callpath-service:

Callpath
--------------------------------

The `callpath` service uses libunwind to add callpaths to Caliper
context snapshots. By default, the callpath service provides
call-stack addresses. Set ``CALI_CALLPATH_USE_NAMES=true`` to retrieve
function names on-line. Call-path addresses are provided in the
``callpath.address`` attribute, call-path region names in
``callpath.regname``. Example:

.. code-block:: sh

  $ export CALI_SERVICES_ENABLE=callpath:event:recorder:trace
  $ export CALI_CALLPATH_USE_NAME=true
  $ export CALI_CALLPATH_USE_ADDRESS=true
  $ ./test/cali-basic
  $ cali-query -e --print-attributes=callpath.address:callpath.regname
  $ callpath.address=401207/2aaaac052d5d/400fd9,callpath.regname=main/__libc_start_main/_start

The example shows the ``callpath.address`` and ``callpath.regname``
attributes in Caliper context records.

.. envvar:: CALI_CALLPATH_USE_NAME=(true|false)

   Provide region names for call paths. Incurs higher overhead. Note
   that region names for C++ and Fortran functions are not demangled.

   Default: false.

.. envvar:: CALI_CALLPATH_USE_ADDRESS=(true|false)

   Record region addresses for call paths.

   Default: true.

.. envvar:: CALI_CALLPATH_SKIP_FRAMES=<number of frames>

   Skip a number of stack frames. This avoids recording stack frames
   within the Caliper library.

   Default: 10

Environment Information
--------------------------------

The environment information (`env`) service collects runtime
environment information at process startup and adds it to the Caliper
context.

Specifically, it collects

 * The process' command line (program name and arguments)
 * Machine type and hostname, and operating system type, release, and
   version
 * Date and time of program start in text form

Moreover, the environment information service can put any environment
variable defined at program start on the Caliper blackboard.

.. envvar:: CALI_ENV_EXTRA=(variable1:variable2:...)

   List of extra environment variables to import.

   Default: empty

.. _event-service:

Event
--------------------------------

The event trigger service triggers snapshots when attributes are
updated. It is possible to select a list of snapshot-triggering
attributes, or have any attribute update trigger snapshots.
Updates of attributes with the ``CALI_ATTR_SKIP_EVENTS`` property will
never trigger snapshots.

Snapshots triggered by the event service include an attribute which
describes the event that triggered the snapshot, in the following
form: ::

  event.<begin|set|end>#<attribute name>=<value>

For example, a snapshot triggered by the call
``cali_set_int_byname("my.iteration", 42);`` includes the attribute
`event.set#my.iteration=42` to describe the triggering event.

The following example shows the snapshots generated by the example
program in :doc:`usage`:

.. code-block:: sh

                $ export CALI_SERVICES_ENABLE=event:recorder:trace
                $ ./test/cali-basic
                $ cali-query -e 150819-113409_40027_W5Z0mWvoJUyn.cali
                event.begin#initialization=true,cali.snapshot.event.begin=39
                event.end#initialization=true,cali.snapshot.event.end=39,initialization=true
                event.begin#loop=true,cali.snapshot.event.begin=53
                event.set#iteration=0,cali.snapshot.event.set=63,loop=true
                event.set#iteration=1,cali.snapshot.event.set=63,iteration=0,loop=true
                event.set#iteration=2,cali.snapshot.event.set=63,iteration=1,loop=true
                event.set#iteration=3,cali.snapshot.event.set=63,iteration=2,loop=true
                event.end#iteration=3,cali.snapshot.event.end=63,iteration=3,loop=true
                event.end#loop=true,cali.snapshot.event.end=53,loop=true

By setting ``CALI_EVENT_TRIGGER``, we can configure the example to
only trigger snapshot for "iteration" attribute updates:

.. code-block:: sh

                $ export CALI_SERVICES_ENABLE=event:recorder:trace
                $ export CALI_EVENT_TRIGGER=iteration
                $ ./test/cali-basic
                $ cali-query -e 150819-113409_40027_W5Z0mWvoJUyn.cali
                event.set#iteration=0,cali.snapshot.event.set=63,loop=true
                event.set#iteration=1,cali.snapshot.event.set=63,iteration=0,loop=true
                event.set#iteration=2,cali.snapshot.event.set=63,iteration=1,loop=true
                event.set#iteration=3,cali.snapshot.event.set=63,iteration=2,loop=true
                event.end#iteration=3,cali.snapshot.event.end=63,iteration=3,loop=true

.. envvar:: CALI_EVENT_TRIGGER=(attribute1:attribute2:...)

   List of attributes that trigger measurement snapshots.
   If empty, all user attributes trigger snapshots.

   Default: empty

Debug
--------------------------------

The debug service prints an event log on the selected Caliper log
stream. This is useful to debug source-code annotations. Note that you
need to set Caliper's verbosity level to at least 2 to see the log
output.

Example:

.. code-block:: sh

                $ export CALI_SERVICES_ENABLE=debug
                $ export CALI_LOG_VERBOSITY=2
                $ ./test/cali-basic
                == CALIPER: Available services: callpath papi debug event pthread recorder timestamp mpi
                == CALIPER: Registered debug service
                == CALIPER: Initialized
                ...
                == CALIPER: Event: create_attribute (attr = phase)
                == CALIPER: Event: pre_begin (attr = phase)
                == CALIPER: Event: pre_begin (attr = phase)
                == CALIPER: Event: pre_end (attr = phase)
                == CALIPER: Event: pre_begin (attr = phase)
                == CALIPER: Event: create_attribute (attr = iteration)
                == CALIPER: Event: pre_set (attr = iteration)
                == CALIPER: Event: pre_set (attr = iteration)
                == CALIPER: Event: pre_set (attr = iteration)
                == CALIPER: Event: pre_set (attr = iteration)
                == CALIPER: Event: pre_end (attr = iteration)
                == CALIPER: Event: pre_end (attr = phase)
                == CALIPER: Event: pre_end (attr = phase)
                == CALIPER: Event: finish
                == CALIPER: Finished

.. _libpfm-service:

Libpfm
--------------------------------

The libpfm service performs per-thread event-based sampling. The user
may configure the event upon which to sample, the values to record for
each sample, and the sampling period.

.. envvar:: CALI_LIBPFM_EVENTS

   Comma-separated list of events to sample. Event names are resolved
   through libpfm, and may include software and hardware events (see
   libpfm's showevtinfo tool
   https://sourceforge.net/p/perfmon2/libpfm4/ci/master/tree/examples/
   to obtain a list of events available on a particular system).

   Default: cycles

.. envvar:: CALI_LIBPFM_ENABLE_SAMPLING

   Whether to record event samples. If set, will trigger a snapshot
   containing all sampled attributes listed in 
   CALI_LIBPFM_SAMPLE_ATTRIBUTES after CALI_SAMPLE_PERIOD events have 
   occurred.

   Default: true

.. envvar:: CALI_LIBPFM_RECORD_COUNTERS

    If set, counter values of all active events will be recorded
    at every Caliper snapshot.

    Default: true

.. envvar:: CALI_LIBPFM_SAMPLE_ATTRIBUTES

   Comma-separated list of attributes to record for each sample.

   Available entries are:
     ip             Instruction pointer
     id             Sample id
     stream_id      Stream id
     time           Timestamp
     tid            Thread id
     period         Current sampling period
     cpu            CPU
     transaction    Type of transaction
     addr           Data address*
     weight         Latency*
     data_src       Encoding for memory resource (L1, L2, DRAM etc.)*

     \*available only for certain events.

   Default: ip,time,tid,cpu

.. envvar:: CALI_LIBPFM_PERIOD

   Sampling period for each event (valid when sampling is enabled). 
   When set to a value N, a sample will  be recorded after every N 
   number of events has occurred.

   For multiple events, this should be a comma-separated list for the 
   periods of respective events.

   Default: 20000000

.. envvar:: CALI_LIBPFM_PRECISE_IP

   Whether to set (precise) for events that support precise ip. Some
   events require (precise) to be set, for others it is optional (see
   output of libpfm's showevtinfo tool
   https://sourceforge.net/p/perfmon2/libpfm4/ci/master/tree/examples/
   to determine when it is available or required).

   For multiple events, this should be a comma-separated list for the 
   precise_ip values of respective events.

   May be set to either 0, 1, or 2.

   Default: 0

.. envvar:: CALI_LIBPFM_CONFIG1

   Extra event configuration. Some events require an additional
   parameter to configure behavior, such as latency threshold (see
   output of libpfm's showevtinfo tool
   https://sourceforge.net/p/perfmon2/libpfm4/ci/master/tree/examples/
   to determine when it is available or required).

   For multiple events, this should be a comma-separated list for the 
   config1 values of respective events.

   Default: 0

The following example shows how to configure PEBS memory access sampling
with a latency threshold (available on SandyBridge, IvyBridge,
Haswell):

.. code-block:: sh

   CALI_LIBPFM_EVENTS=MEM_TRANS_RETIRED:LATENCY_ABOVE_THRESHOLD
   CALI_LIBPFM_PERIOD=100
   CALI_LIBPFM_PRECISE_IP=2
   CALI_LIBPFM_CONFIG1=100
   CALI_LIBPFM_SAMPLE_ATTRIBUTES=ip,time,tid,cpu,addr,weight

.. _mpi-service:

MPI
--------------------------------

The MPI service records MPI operations and the MPI rank. Use it to
keep track of the program execution spent in MPI. You can select the
MPI functions to track by setting ``CALI_MPI_WHITELIST`` or
``CALI_MPI_BLACKLIST`` filters. By default, all MPI functions are
instrument.

MPI function names are stored in the ``mpi.function`` attribute, and
the MPI rank in the ``mpi.rank`` attribute.

Note that you have to link the `libcaliper-mpiwrap` library with the
application in addition to the regular Caliper libraries to obtain MPI
information.

.. envvar:: CALI_MPI_WHITELIST

   Comma-separated list of MPI functions to instrument. 
   Only whitelisted functions will be instrumented. Set to ``all`` 
   to instrument all MPI functions.

   Default: Empty

.. envvar:: CALI_MPI_BLACKLIST

   Comma-separated list of MPI functions that will *not* be
   instrumented.

   Note: if both whitelist and blacklist are set, only whitelisted
   functions will be instrumented, and the blacklist will be applied
   to the whitelisted functions.

   Default: Empty

MPIT
--------------------------------

The MPIT service records MPI performance variables (PVARs) exported by
the MPI 3 tools interface (MPI-T). Users can provide a list of PVARs
to record. By default, all available PVARs will be recorded.  Note
that PVARs are MPI implementation specific; MPI implementations do not
necessarily export the same or similar PVARs, or any at all.

This service is part of the Caliper MPI wrapper library
(`libcaliper-mpiwrap`), which must be linked to the application in
addition to the regular Caliper runtime library.

.. envvar:: CALI_MPIT_PVARS

   A comma-separated list of PVARs to export. PVAR names are defined
   by the MPI implementation. Default: empty, records all available
   PVARs.

The :ref:`mpi <mpi-service>` service must be enabled for mpit
to work.

.. _mpireport-service:

MPI Report
--------------------------------

The MPI report service (`mpireport`) aggregates, formats, and writes
collected Caliper records from all ranks in an MPI program into a
single global report.  By default, the mpireport service prints a
tabular, human-readable report of the collected snapshots. Users can
provide a query specification in CalQL syntax to define filter,
aggregation, and formatting options.

The mpireport service aggregates Caliper data across MPI processes
before printing it. This happens on every Caliper flush event.
Enabling the mpireport service will trigger a flush during
MPI_Finalize.

The :ref:`mpi <mpi-service>` service must be enabled for mpireport
to work.

.. envvar:: CALI_MPIREPORT_FILENAME

   File name of the output file. May be set to ``stdout`` or ``stderr``
   to print to the standard output or error streams, respectively.

   Similar to the `recorder` service, the file name may contain fields
   which will be substituted by attribute values (see `recorder`
   service description).

   Default: stdout

.. envvar:: CALI_MPIREPORT_CONFIG

   An aggregation and formatting specification in CalQL syntax.

   Default: empty; all attributes in the snapshots will be printed.

.. _papi-service:

PAPI
--------------------------------

The PAPI service collects hardware counter information through the
PAPI library.

.. envvar:: CALI_PAPI_COUNTERS

   The PAPI counters to read as comma-separated list.
   Available counters can be found with the `papi_avail` command
   provided by PAPI. If successfull, snapshots will contain
   attributes named ``papi.COUNTER_NAME``. Their value represents the
   increase of that counter since the previous snapshot.

Example:

.. code-block:: sh

   $ CALI_SERVICES_ENABLE=event,papi,trace,report
   $ CALI_PAPI_COUNTERS=PAPI_TOT_CYC,PAPI_L1_DCM
   $ ./test/cali-basic
   papi.PAPI_TOT_CYC papi.PAPI_L1_DCM function annotation loop     iteration#mainloop
               36146              431
               28328              372 main
               20601              311 main     init
               37010              546 main
                7147              150 main                mainloop
                8425              115 main                mainloop                  0

.. _pthread-service:

Pthread
--------------------------------

The Pthread service wraps `pthread_create` using GOTCHA, and adds a
``pthread.id`` attribute with a numeric thread ID for the new child
thread. In doing so, the pthread service automatically creates a
Caliper thread scope on the child thread: this is useful to
automatically start sampling (e.g. with the `sampler` service) on each
new thread.

.. _recorder-service:

Recorder
--------------------------------

The recorder service writes Caliper snapshot records into a file
using a custom text-based I/O format. These files can be read
with the `cali-query` tool.

Writing occurs during a flush phase, which prompts snapshot-buffering
services (in particular, the `trace` or `aggregate` services) to push
out buffered snapshot records for writing. A flush phase can take
several seconds and significantly disrupt program performance. By
default, Caliper initiates a flush at the end of the program
execution.

You can also set the directory and filename that should be used;
by default, the recorder service will auto-generate a
file name.

.. envvar:: CALI_RECORDER_FILENAME=(stdout|stderr|filename pattern)

   File name of the output file. May be set to ``stdout`` or ``stderr``
   to print to the standard output or error streams, respectively.

   The file name string can contain fields, denoted by ``%attribute_name%``,
   which will be replaced with attribute values. For example, in an MPI
   program with the `mpi` service enabled, the string ``caliper-%mpi.rank%.cali``
   will create files ``caliper-0.cali``, ``caliper-1.cali``, etc. for each
   mpi rank. For this to work, the attributes named in the fields need to
   be set on the blackboard during the flush phase.

   Default: not set, auto-generates a unique file name.

.. envvar:: CALI_RECORDER_DIRECTORY=(directory name)

   Directory to write context trace files to. The directory must exist,
   Caliper does not create it. Default: not set, use current working
   directory.

.. _report-service:

Report
--------------------------------

The report service aggregates, formats, and writes collected Caliper
records into files or stdout on Caliper flush events (typically, at
program end). By default, the report service prints a tabular,
human-readable report of the collected snapshots. Users can provide a
query specification in CalQL syntax to define filter, aggregation, and
formatting options.


.. envvar:: CALI_REPORT_FILENAME

   File name of the output file. May be set to ``stdout`` or ``stderr``
   to print to the standard output or error streams, respectively.

   Similar to the `recorder` service, the file name may contain fields
   which will be substituted by attribute values (see `recorder`
   service description), for example to create individual
   ``report-0.txt``, ``report-1.txt`` etc. files for each rank in a
   multi-process program.

   Default: stdout

.. envvar:: CALI_REPORT_CONFIG

   A formatting specification in CalQL syntax.

   Default: empty; all attributes in the snapshots will be printed.

Example: Consider the following report configuration and list of
flushed snapshots: ::

   CALI_REPORT_CONFIG="SELECT function,time.duration WHERE phase=loop ORDER BY time.duration FORMAT table"

   phase=init,function=foo,time.duration=42
   phase=loop,function=foo,time.duration=2000
   phase=loop,function=bar,time.duration=12
   phase=loop,function=foo,time.duration=100

This configuration will create the following report output: ::

   function time.duration
   bar                 12
   foo                100
   foo		     2000

Only snapshots where ``phase=loop`` are selected (due to the filter
configuration), and the ``function`` and ``time.duration`` attributes
are printed, in ascending order of ``time.duration``.

.. _sampler-service:

Sampler
--------------------------------

The sampler service implements time-based sampling. It triggers
snapshots at regular intervals. Sampling allows for low-overhead
performance data collection, and can provide insights into code
regions that are not or only sparsely covered with source-code
annotations. 

Caliper must be initialized on each thread that should be
sampled. This can be done explicitly via the annotation API, or via
the :ref:`pthread <pthread-service>` service for child threads.

.. envvar:: CALI_SAMPLER_FREQUENCY

   Sampling frequency in Hz. Default: 10

When active, the sampler service regularly triggers snapshots with the
specified frequency. Each snapshot triggered by the sampler service
contains a ``cali.sampler.pc`` attribute with the program address
where the target program was interrupted. The symbollookup service can
use this to retrieve function name as well as source file and line
information.

The following example generates a sampling trace at 100Hz, uses the
symbollookup service to retrieve function name information, and prints
a flat profile of the number of samples per function::

    CALI_SERVICES_ENABLE=report,sampler,symbollookup,trace
    CALI_SAMPLER_FREQUENCY=100
    CALI_REPORT_CONFIG="SELECT source.function#cali.sampler.pc,count() GROUP BY source.function#cali.sampler.pc FORMAT table ORDER BY count DESC"

.. _symbollookup-service:

Symbollookup
--------------------------------

The symbollookup service provides function name, source file, and
source line number lookup for binary program addresses from, e.g.,
stack unwinding or program counter sampling. The symbol lookup takes
place during snapshot buffer flushes. It appends symbol attributes for
each address attribute to the snapshots being flushed. For an address
attribute ``address``, the function, file, and line number will be
added in ``source.function#address``, ``source.file#address``, and
``source.line#address`` attributes, respectively. If a symbol lookup
was unsuccessful for any reason, the value is set to `UNKNOWN`.

.. envvar:: CALI_SYMBOLLOOKUP_ATTRIBUTES

   Explicitly select address attributes for which to perform symbol
   lookups. Colon-separated list. Default: empty, selects address
   attributes automatically via `class.symboladdress` attribute class.

.. envvar:: CALI_SYMBOLLOOKUP_LOOKUP_FUNCTIONS

   Perform function name lookup. `TRUE` or `FALSE`, default `TRUE`.

.. envvar:: CALI_SYMBOLLOOKUP_LOOKUP_SOURCELOC

   Perform source file and line number lookup. `TRUE` or `FALSE`,
   default `TRUE`.

Sysalloc
--------------------------------

The sysalloc service wraps `malloc`, `free`, `calloc`, and `realloc`
memory allocation calls, and marks the allocated memory regions so
they can be tracked with the alloc service.

Textlog
--------------------------------

The textlog service prints a text representation of snapshots to a configurable
output stream. This can be used to print out a log of the program's
progress at runtime.

Currently, text log output can only be triggered by attribute update events.
Therefore, the `event` service must be active as well.
You can select which attribute updates trigger a text log output, define the
output format, and set the output stream (stdout, stderr, or a file name).

The following example prints a text log for the `phase` attribute of the
test application with Caliper's auto-generated format string:

.. code-block:: sh

      $ export CALI_SERVICES_ENABLE=event:textlog:timestamp
      $ export CALI_TEXTLOG_TRIGGER=phase
      $ ./test/cali-basic
      == CALIPER: Registered event trigger service
      == CALIPER: Registered timestamp service
      == CALIPER: Registered text log service
      == CALIPER: Initialized
      phase=main/init                                                       21
      phase=main/loop                                                       84
      phase=main                                                            219
      == CALIPER: Finished


.. envvar:: CALI_TEXTLOG_TRIGGER=attr1:attr2:...

   Select attributes which trigger a text log output. Note that the `event`
   service must be active in order to trigger snapshots in the first place,
   and the attributes selected here must be in the list of attributes that
   trigger snapshots (defined by `CALI_EVENT_TRIGGER`).

.. envvar:: CALI_TEXTLOG_FORMATSTRING=(formatstring)

   Define what to print. The formatstring can contain fields, denoted by
   ``%attribute_name%``, which prints the value of an attribute. Optionally,
   a field can contain a width specification, denoted by ``[width]``, to set
   the minimum width of a field. Any other text is printed verbatim.
   For example, ``Phase: %[32]app.phase% %[6]time.phase.duration%`` writes
   log strings with two fields: the value of the `app.phase` attribute with
   a minimum width of 40 characters, and the value of  `time.phase.duration`
   attribute with a minimum width of 6 characters, respectively. A resulting
   log entry might look like this:

   .. code-block:: sh

      Phase: main/loop                       7018

   Default: empty; Caliper will automatically create a format string based on
   the selected trigger attributes.

.. envvar:: CALI_TEXTLOG_FILENAME=(stdout|stderr|filename)

   File name for the text log. May be set to ``stdout`` or ``stderr``
   to print to the standard output or error streams, respectively.

   Default: stdout

.. _timestamp-service:

Timestamp
--------------------------------

The timestamp service adds a time offset, timestamp, or duration to
context records. Note that timestamps are *not* synchronized between
nodes in a distributed-memory program.

.. envvar:: CALI_TIMER_SNAPSHOT_DURATION=(true|false)

   Measure duration (in microseconds) of the context epoch (i.e., the
   time between two consecutive context snapshots). The value will be
   saved in the snapshot record as attribute ``time.duration``.

   Default: false

.. envvar:: CALI_TIMER_OFFSET=(true|false)

   Include the time offset (time since program start, in microseconds)
   with each context snapshot. The value will be saved in the snapshot
   record as attribute ``time.offset``.

   Default: false

.. envvar:: CALI_TIMER_TIMESTAMP=(true|false)

   Include absolute timestamp (time since UNIX epoch, in seconds) with
   each context snapshot. The value will be saved in the snapshot record
   as attribute ``time.timestamp``.

.. envvar:: CALI_TIMER_INCLUSIVE_DURATION=(true|false)

   For snapshots triggered by ``set`` or ``end`` events, calculate the
   duration of the corresponding ``begin-end``, ``set-set``, or
   ``set-end`` phase. The value will be saved in the snapshot record
   as attribute ``time.inclusive.duration``.

   The event service with event trigger information generation needs
   to be enabled for this feature.

   Default: true

.. _trace-service:

Trace
--------------------------------

The trace service creates an I/O record for each snapshot. With the
``recorder`` service enabled, this will create a snapshot trace file.

The trace service maintains per-thread snapshot buffers. By default,
trace buffers will grow automatically. This behavior can be changed by
setting a *buffer policy*. There are three options:

Grow
    Grow the buffer when it is full. This is the default.

Stop
    Stop recording when the buffer is full.

Flush
    Flush the buffer when it is full and continue recording. Note that
    buffer flushes can significantly perturb the program's
    performance.

.. envvar:: CALI_TRACE_BUFFER_SIZE

   Size of the trace buffer, in Megabytes. With the `grow` buffer
   policy, this is the size of a trace buffer *chunk*: When the buffer
   is full, another chunk of this size is added.

   Default: 2 (MiB).

.. envvar:: CALI_TRACE_BUFFER_POLICY

   Sets the trace buffer policy (see above). Either `grow`, `stop`, or
   `flush`.

   Default: `grow`.
