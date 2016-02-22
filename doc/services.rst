Caliper services
================================

Caliper comes with a number of optional modules (*services*) that
provide measurement data or processing and data recording
capabilities. The flexible combination and configuration of these
services allows you to quickly assemble recording solutions for a wide
range of usage scenarios.

You can enable the services required for your measurement with the
``CALI_SERVICES_ENABLE`` configuration variable, e.g.::

  export CALI_SERVICES_ENABLE=event:pthread:recorder

to create event-triggered context traces for a multi-threaded application.

The following sections describe the available service modules and
their configuration.

Callpath
--------------------------------

The `callpath` service uses libunwind to add callpaths to Caliper
context snapshots. By default, the callpath service provides
call-stack addresses. Set ``CALI_CALLPATH_USE_NAMES=true`` to retrieve
function names on-line. Call-path addresses are provided in the
``callpath.address`` attribute, call-path region names in
``callpath.regname``. Example:

.. code-block:: sh

  $ export CALI_SERVICES_ENABLE=callpath:event:recorder
  $ export CALI_CALLPATH_USE_NAME=true
  $ export CALI_CALLPATH_USE_ADDRESS=true
  $ ./test/cali-basic
  $ cali-query -e --print-attributes=callpath.address:callpath.regname
  $ callpath.address=401207/2aaaac052d5d/400fd9,callpath.regname=main/__libc_start_main/_start

The example shows the ``callpath.address`` and ``callpath.regname``
attributes in Caliper context records.
  
Configuration
................................

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

The environment information (``env``) service collects runtime
environment information at process startup and adds it to the Caliper
context.

Specifically, it collects

 * The process' command line (program name and arguments)
 * Machine type and hostname, and operating system type, release, and
   version
 * Date and time of program start in text form

Moreover, the environment information service can put any environment
variable defined at program start on the Caliper blackboard.

Configuration
................................

.. envvar:: CALI_ENV_EXTRA=(variable1:variable2:...)

   List of extra environment variables to import.

   Default: empty

Event
--------------------------------

The event trigger service triggers context snapshots when attributes
are updated. You can select a list of triggering attributes, or have
any attribute update trigger context snapshots.

Example
................................

All attributes trigger context snapshots:

.. code-block:: sh

                $ export CALI_SERVICES_ENABLE=event:recorder:trace
                $ ./test/cali-basic
                $ cali-query -e 150819-113409_40027_W5Z0mWvoJUyn.cali
                phase=main
                phase=init/main
                phase=main
                phase=loop/main
                iteration=0,phase=loop/main
                iteration=1,phase=loop/main
                iteration=2,phase=loop/main
                iteration=3,phase=loop/main
                phase=loop/main
                phase=main

Only "iteration" attribute updates trigger context snapshots:

.. code-block:: sh
                
                $ export CALI_SERVICES_ENABLE=event:recorder:trace
                $ export CALI_EVENT_TRIGGER=iteration
                $ ./test/cali-basic
                $ cali-query -e 150819-113409_40027_W5Z0mWvoJUyn.cali
                phase=loop/main
                iteration=0,phase=loop/main
                iteration=1,phase=loop/main
                iteration=2,phase=loop/main
                iteration=3,phase=loop/main

Configuration
................................

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

MPI
--------------------------------

The MPI service records MPI operations and the MPI rank. Use it to
keep track of the program execution spent in MPI. You can select the
MPI functions to track by setting ``CALI_MPI_WHITELIST`` or
``CALI_MPI_BLACKLIST`` filters.

MPI function names are stored in the ``mpi.function`` attribute, and
the MPI rank in the ``mpi.rank`` attribute.

Note that you have to link the `libcaliper-mpiwrap` library with the
application in addition to the regular Caliper libraries to obtain MPI
information.

Configuration
................................

.. envvar:: CALI_MPI_WHITELIST=(MPI_Fn_1:MPI_Fn_2:...)
            
   List of MPI functions to instrument. If set, only whitelisted
   functions will be instrumented.

.. envvar:: CALI_MPI_BLACKLIST=(MPI_Fn_1:MPI_Fn_2:...)
            
   List of MPI functions that fill be filtered. Note: if both
   whitelist and blacklist are set, only whitelisted functions will
   be instrumented, and the blacklist will be applied to the
   whitelisted functions.

Pthread
--------------------------------

The `pthread` service manages thread environments for any
pthread-based multi-threading runtime system. A thread environment
manager such as the `pthread` service is responsible for creating
separate per-thread contexts in multithreaded programs.

If you record attributes on multiple threads, it is strongly
recommended to enable the `pthread` service.

Recorder
--------------------------------

The recorder service writes Caliper I/O records into a file.

By default, the recorder service stores records in an
in-memory buffer to avoid application performance perturbance because
of I/O. You can configure the buffer sizes and determine whether they
are allowed to grow. You can also set the directory and filename that
should be used; by default, the recorder service will auto-generate a
file name.

Configuration
................................

.. envvar:: CALI_RECORDER_FILENAME=(stdout|stderr|filename)
            
   File name for context trace. May be set to ``stdout`` or ``stderr``
   to print to the standard output or error streams, respectively.
   
   Default: not set, auto-generates a unique file name.

.. envvar:: CALI_RECORDER_DIRECTORY=(directory name)
            
   Directory to write context trace files to. The directory must exist,
   Caliper does not create it. Default: not set, use current working
   directory.

.. envvar:: CALI_RECORDER_RECORD_BUFFER_SIZE=(number of records)
            
   Initial number of records that can be stored in the in-memory record
   buffer.

   Default: 8000

.. envvar:: CALI_RECORDER_DATA_BUFFER_SIZE=(number of data elements)
            
   Initial number of data elements that can be stored in the in-memory record
   buffer.

   Default: 60000

.. envvar:: CALI_RECORDER_BUFFER_CAN_GROW=(true|false)
            
   Allow record and data buffers to grow if necessary. If false, buffer content
   will be flushed to disk when either buffer is full.
   
   Default: true

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
                

Configuration
................................

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

Timestamp
--------------------------------

The timestamp service adds a time offset, timestamp, or duration to
context records. Note that timestamps are *not* synchronized between
nodes in a distributed-memory program.

Configuration
................................

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

.. envvar:: CALI_TIMER_INCLUSIVE_DURATION
            
   For snapshots triggered by ``set`` or ``end`` events, calculate the
   duration of the corresponding ``begin-end``, ``set-set``, or
   ``set-end`` phase. The value will be saved in the snapshot record
   as attribute ``time.inclusive.duration``.

   The event service with event trigger information generation needs
   to be enabled for this feature.
  
Trace
--------------------------------

The trace service creates an I/O record for each snapshot. With the
``recorder`` sercice enabled, this will create a snapshot trace file.

