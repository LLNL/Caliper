Caliper tools
================================

Caliper comes with additional tool programs for post-processing
the raw Caliper output data (in ``.cali`` files by default).

Caliper's output is in the form of snapshots. Snapshots
hold the information chosen to be recorded by the enabled services.
See :doc:`services` for more on services.

All examples on this page use the sample program `example.cpp`_.


Cali-query
--------------------------------

The ``cali-query`` tool reads the raw snapshot records and can merge data and filter output.

Usage
````````````````````````````````

``cali-query [OPTIONS]... [FILES]...``

Options
````````````````````````````````
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-q`` | ``--query=QUERY_STRING``          | A CalQL expression. Specify the filter, aggregation, and formatting |
|        |                                   | operations to execute as a SQL-like CalQL expression.               |
|        |                                   | See :doc:`calql`.                                                   |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-s`` | ``--select=QUERY_STRING``         | Selects snapshots to expand. Snapshots can be filtered by which     |
|        |                                   | attributes or variables appear, and specific states of attributes or|
|        |                                   | variables. When this is set, all other snapshots are ignored in the |
|        |                                   | ``expand`` command. Snapshots with specific attributes and variables|
|        |                                   | or values thereof can be excluded by using a ``-`` symbol in front  |
|        |                                   | of the name of the attribute/variable. Specific attribute or        |
|        |                                   | variable values are listed in ``attribute=value`` format. Multiple  |
|        |                                   | attributes/variables are selected by listing with a ``,`` separator.|
|        |                                   | The default behavior is to select all snapshots.                    |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-t`` | ``--table``                       | Print snapshots in a human-readable table format.                   |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-S`` | ``--sort-by=ATTRIBUTES``          | Sort snapshots by the given attributes when printing a table.       | 
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-e`` | ``--expand``                      | Expands the selected snapshots (from ``-s``) and prints the selected|
|        |                                   | attributes (from ``--print-attributes``) as lists of comma-separated|
|        |                                   | key-value pairs (e.g., ``attribute1=value1,...``. Default behavior  |
|        |                                   | is to expand and print all snapshots and attributes.                |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-p`` | ``--print-attributes=ATTRIBUTES`` | Select which attributes to print with the ``--expand`` or           |
|        |                                   | ``--table`` formatters.                                             |
|        |                                   | Attributes can be                                                   |
|        |                                   | excluded by using a ``-`` symbol in front of the name. Multiple     |
|        |                                   | attributes are selected/excluded by listing with a ``,`` separator. |
|        |                                   |                                                                     |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-a`` | ``--aggregate=AGGREGATION_OPS``   | Aggregate over the specified attributes with the specified          |
|        |                                   | operation(s). ``AGGREGATION_OPS`` format is:                        |
|        |                                   | ``(operation(attr1)|operation(attr1)),(operation(attr2)),...``      |
|        |                                   | Operations available: ``sum(attr)``, ``max(attr)``, ``min(attr)``,  |
|        |                                   | ``count``.                                                          |
+--------+-----------------------------------+---------------------------------------------------------------------+
|        | ``--aggregate-key=ATTRIBUTES``    | Collapses previously aggregated snapshots, using ``--aggregate``,   |
|        |                                   | by the specified attributes. ``ATTRIBUTES`` is of the form:         |
|        |                                   | ``attr1,attr2,...`` where ``attribute#value`` may be used.          |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-f`` | ``--format=FORMAT_STRING``        | Print the snapshot data in a table in the format specified by       |
|        |                                   | ``FORMAT_STRING``. ``FORMAT_STRING`` should be of the form:         |
|        |                                   | ``%[width1]attr1% %[width2]attr2% ...``, where ``width`` is the     |
|        |                                   | minimum width in characters of the printed value for the matching   |
|        |                                   | attribute, ``attr``. Limits the printed attributes to those         |
|        |                                   | listed in ``FORMAT_STRING``. Will override the ``--expand`` option. |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-T`` | ``--title=TITLE_STRING``          | Specify a custom title (header line) for formatted (``-f``) output. |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-o`` | ``--output=FILE``                 | Set the name of the output file.                                    |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-h`` | ``--help``                        | Print the help message, a summary of these options.                 |
+--------+-----------------------------------+---------------------------------------------------------------------+

Files
````````````````````````````````
The files used by ``cali-query`` are ``.cali`` record files produced from running Caliper
in a program. Multiple ``.cali`` files can be read at once; ``cali-query`` will merge their
contents into a single output stream.

Examples
````````````````````````````````

``cali-query`` can only be used on the ``.cali`` files created by using a configured
Caliper library in a program. The program example.cpp_ was run:

.. code-block:: sh

    $ CALI_SERVICES_ENABLE=event,recorder,timestamp,trace example
    == CALIPER: Registered event trigger service
    == CALIPER: Registered recorder service
    == CALIPER: Registered timestamp service
    == CALIPER: Registered trace service
    == CALIPER: Initialized
    b = 34
    == CALIPER: Trace: Flushed 45 snapshots.
    == CALIPER: Wrote 169 records.

and produced the following file::

    160809-094411_72298_fuu1NeAHT2US.cali

For comparison, here are the first six lines of records from this file:

.. code-block:: none

    __rec=node,id=8,attr=8,data=cali.attribute.name,parent=3
    __rec=node,id=9,attr=8,data=cali.attribute.type,parent=7
    __rec=node,id=10,attr=8,data=cali.attribute.prop,parent=1
    __rec=node,id=13,attr=10,data=12,parent=3
    __rec=node,id=14,attr=8,data=cali.caliper.version,parent=13
    __rec=node,id=11,attr=10,data=148,parent=0

Basic Use
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: sh

    $ cali-query -o cali-query.output 160809-094411_72298_fuu1NeAHT2US.cali    
    == CALIPER: Initialized

Note that ``-o`` is being used to control the output in all examples.

Without selection, aggregation, or formatting options, ``cali-query`` 
merges the input files and writes out an output stream in its native .cali
format. 
The first six lines of records after processing look like this:

.. code-block:: none

    __rec=node,id=8,attr=8,data=cali.attribute.name,parent=3
    __rec=node,id=9,attr=8,data=cali.attribute.type,parent=7
    __rec=node,id=10,attr=8,data=cali.attribute.prop,parent=1
    __rec=node,id=11,attr=10,data=12,parent=3
    __rec=node,id=12,attr=8,data=cali.caliper.version,parent=11
    __rec=node,id=13,attr=10,data=148,parent=0


CalQL expressions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

An easy way to specify query and report options for cali-query is the
SQL-like CalQL language (see :doc:`calql`). For example, filtering and
aggregating trace records to print a table report can be accomplished
with the following expression:

.. code-block:: sh

   $ cali-query -q "SELECT *,sum(time.inclusive.duration) WHERE main=loop FORMAT table" *.cali

   
``-t`` / ``--table``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``--table`` option prints snaphsots in a human-readable table format. For example, 

.. code-block:: sh

    $ cali-query -t 160809-094411_72298_fuu1NeAHT2US.cali

will print snapsots as follows:

.. code-block:: none

    main            time.inclusive.duration iteration factorial
    init                               1813
    body
    body/init                           114
    body/loop
    body/loop                                       0
    body/loop                           215         0 init
    body/loop                            21         0 comp
    body/loop                           529         0
    body/loop                                       1

The table's columns represent attributes. Each row represents a single snapshot.

The ``--print-attributes`` specifies the list of attributes to
print, and the order in which they are shown. By default, the table includes all
attributes except certain Caliper-internal attributes whose names start
with "event." or "cali.".

The ``--sort-by`` option specifies a list of attributes to be used as sorting
criteria. If given, rows will be printed in ascending order according to the value
of the given attributes:

.. code-block:: sh

    $ cali-query -t 160809-094411_72298_fuu1NeAHT2US.cali --sort-by=time.inclusive.duration

.. code-block:: none
                
    main            time.inclusive.duration iteration factorial
    ...
    body/init                           114
    body/loop                           164         3
    body/loop                           183         2
    body/loop                           210         4
    body/loop                           215         0 init
    body/loop                           529         0
    body/loop                          1214
    init                               1813
    

``-e`` and ``--expand``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``-e`` or ``--expand`` option expands snapshot records into
comma-separated `attribute=value` lists.

.. code-block:: sh

    $ cali-query -e -o cali-query-expanded.output 160809-094411_72298_fuu1NeAHT2US.cali
    == CALIPER: Initialized

The first six lines of records after expansion look like this:

.. code-block:: none

    event.begin#main=init,cali.snapshot.event.begin=38,cali.snapshot.event.attr.level=1,cali.caliper.version=1.5.dev
    event.set#main=body,cali.snapshot.event.set=38,cali.snapshot.event.attr.level=1,main=init,cali.caliper.version=1.5.dev,time.inclusive.duration=1813
    event.begin#main=init,cali.snapshot.event.begin=38,cali.snapshot.event.attr.level=2,main=body,cali.caliper.version=1.5.dev
    event.set#main=loop,cali.snapshot.event.set=38,cali.snapshot.event.attr.level=1,main=body/init,cali.caliper.version=1.5.dev,time.inclusive.duration=114
    event.set#iteration=0,cali.snapshot.event.set=60,cali.snapshot.event.attr.level=1,main=body/loop,cali.caliper.version=1.5.dev
    event.begin#factorial=init,cali.snapshot.event.begin=70,cali.snapshot.event.attr.level=1,iteration=0,main=body/loop,cali.caliper.version=1.5.dev

The records are now in a more readable, parsable form. All examples following will
be using the ``-e`` option.

``-s`` and ``--select``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``-s`` and ``--select`` expands and/or prints only records that contain the selected attributes.

.. code-block:: sh

    $ cali-query -s "iteration=3,factorial" -e -o cali-query-selected.output 160809-094411_72298_fuu1NeAHT2US.cali
    == CALIPER: Initialized

Here, only the records that contain the ``iteration=3`` attribute value and the ``factorial`` attribute
are expanded and written. These are the first six lines of records written

.. code-block:: none
                
     event.set#factorial=comp,cali.snapshot.event.set=70,cali.snapshot.event.attr.level=1,factorial=init,iteration=3,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=15
     event.begin#factorial=init,cali.snapshot.event.begin=70,cali.snapshot.event.attr.level=2,factorial=comp,iteration=3,main=body/loop,cali.caliper.version=1.5.dev
     event.set#factorial=comp,cali.snapshot.event.set=70,cali.snapshot.event.attr.level=1,factorial=comp/init,iteration=3,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=36
     event.begin#factorial=init,cali.snapshot.event.begin=70,cali.snapshot.event.attr.level=2,factorial=comp/comp,iteration=3,main=body/loop,cali.caliper.version=1.5.dev
     event.set#factorial=comp,cali.snapshot.event.set=70,cali.snapshot.event.attr.level=1,factorial=comp/comp/init,iteration=3,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=37
     event.end#factorial=comp,cali.snapshot.event.end=70,cali.snapshot.event.attr.level=1,factorial=comp/comp/comp,iteration=3,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=14


``-a``/ ``--aggregate`` and ``--aggregate-key``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``-a`` and ``--aggregate`` will give the appropriate values of the operations in ``AGGREGATION_OPS`` for the specified attribute(s)
performed over all snapshots with a matching key.

.. code-block:: sh

    $ cali-query -a "sum(time.inclusive.duration)" -e -o cali-query-aggregated.output 160809-094411_72298_fuu1NeAHT2US.cali
    == CALIPER: Initialized

This is the last six records after aggregation of the ``time.inclusive.duration`` attribute:

.. code-block:: none

    event.end#factorial=comp,cali.snapshot.event.end=70,cali.snapshot.event.attr.level=1,factorial=comp/comp/comp/comp,iteration=4,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=14
    event.set#iteration=2,cali.snapshot.event.set=60,cali.snapshot.event.attr.level=1,iteration=1,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=69
    event.set#iteration=3,cali.snapshot.event.set=60,cali.snapshot.event.attr.level=1,iteration=2,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=183
    event.set#iteration=4,cali.snapshot.event.set=60,cali.snapshot.event.attr.level=1,iteration=3,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=164
    event.end#iteration=4,cali.snapshot.event.end=60,cali.snapshot.event.attr.level=1,iteration=4,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=210
    event.end#main=conclusion,cali.snapshot.event.end=38,cali.snapshot.event.attr.level=1,main=body/conclusion,cali.caliper.version=1.5.dev,time.inclusive.duration=37

``--aggregate-key`` will collapse the aggregated value(s) by what attributes you select with ``ATTRIBUTES``. 

.. code-block:: sh

    $ cali-query -a "count,sum(time.inclusive.duration)" --aggregate-key="event.end#main,event.end#factorial" -e 160809-094411_72298_fuu1NeAHT2US.cali

The ``event.end`` attributes work in ``--aggregate-key`` to add up all of the values for the listed attribute:

.. code-block:: none

    aggregate.count=33,time.inclusive.duration=4852
    event.end#factorial=comp,aggregate.count=11,time.inclusive.duration=79
    event.end#main=conclusion,aggregate.count=1,time.inclusive.duration=37

Note the first line, with no attribute listed. That is every other call of ``time.inclusive.duration`` that does not fall into
the listed attributes, ``event.end#main`` and ``event.end#factorial``, added together.

Options available for ``--aggregate`` are:

+---------------+-----------------------------------------------------------+
| ``sum(attr)`` | Sum up the values of the designated attribute.            |
+---------------+-----------------------------------------------------------+
| ``count``     | Total the number of times the attribute is called.        |
+---------------+-----------------------------------------------------------+
| ``min(attr)`` | Display just the minimum value of the selected attribute. |
+---------------+-----------------------------------------------------------+
| ``max(attr)`` | Display just the maximum value of the selected attribute. |
+---------------+-----------------------------------------------------------+

``--print-attributes``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
``--print-attributes`` controls which parts of each record are written to the output
with the ``--expand`` and ``--table`` formatters.
Only the selected attributes will print in each record.

.. code-block:: sh

    $ cali-query -a "time.inclusive.duration" --print-attributes="main,iteration,time.inclusive.duration" -e -o cali-query-print-aggregated.output 160809-094411_72298_fuu1NeAHT2US.cali
    == CALIPER: Initialized

This is the last six records as above, but printing only the
``main``, ``iteration``, and ``time.inclusive.duration`` attributes::

    iteration=4,main=body/loop,time.inclusive.duration=14
    iteration=1,main=body/loop,time.inclusive.duration=69
    iteration=2,main=body/loop,time.inclusive.duration=183
    iteration=3,main=body/loop,time.inclusive.duration=164
    iteration=4,main=body/loop,time.inclusive.duration=210
    main=body/conclusion,time.inclusive.duration=37


``-f`` and ``--format``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
``-f`` and ``--format`` can be used to specify a custom output format
for snapshots, including limiting the printed attributes. The output
format is specified with a format string of the form
``%[width1]attr1% %[width2]attr2% ...``.
.. code-block:: sh

    $ cali-query -f "%[10]main% %[20]factorial% %[10]time.inclusive.duration%" -o cali-print-agg-format.output cali-query-aggregated.output

The title line and first eight records are formatted as specified::

    init                                1813
    body/init                           114
    body/loop                           1214
    body/loop                           529
    body/loop    init                   215
    body/loop    init                   15
    body/loop    init                   15 
    body/loop    comp/init              101

``-T`` and ``--title``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Prints a custom title string for the ``--format`` output option. The format is any string.

.. code-block:: sh

    $ cali-query -f "%[10]main% %[20]factorial% %[10]time.inclusive.duration%" -t "Main         Factorial              Time" -o cali-print-agg-format-title.output cali-query-aggregated.output

This is the same as the ``--format`` example, but with a custom title::

    Main         Factorial              Time 
    init                                1813
    body/init                           114
    body/loop                           1214
    body/loop                           529
    body/loop    init                   215
    body/loop    init                   15
    body/loop    init                   15
    body/loop    comp/init              101


Cali-stat
--------------------------------

Collect and print statistics about data elements in Caliper streams.
Currently an internal debugging tool for the Caliper context tree.

Usage
````````````````````````````````
``cali-stat [OPTIONS]... [FILES]...``

Options
````````````````````````````````
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-r`` | ``--reuse-statistics``            | Prints statistics about the reuse of the branches of the snapshot   |
|        |                                   | record tree. More reuse is more efficient.                          |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-o`` | ``--output=FILE``                 | Set the name of the output file.                                    |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-h`` | ``--help``                        | Print the help message, a summary of these options.                 |
+--------+-----------------------------------+---------------------------------------------------------------------+

Files
````````````````````````````````
The files used by ``cali-stat`` are ``.cali`` record files produced from running Caliper
in a program. Multiple ``.cali`` files can be read at once.

Examples
````````````````````````````````

Basic Use
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
.. code-block:: sh

    $ cali-stat -o cali-stat.output 160809-094411_72298_fuu1NeAHT2US.cali 
    == CALIPER: Initialized

The report generated::

    Number of records
    Total          Nodes          Snapshots
    169            124            45              
    
    Number of elements
    Total          Nodes          Tree refs      Direct val
    680            496            134            50             
    
    Data size (est.)
    Total          Nodes          Snapshots
    5.26172KiB     4.01953KiB     1.24219KiB     
    
    Elements/snapshot
    Min            Max            Average
    2              5              4.08889        
    
    Attributes referenced in snapshot records
    Total          Average        Refs/Elem
    381            8.46667        0.560294  


``-r`` and ``--reuse-statistics``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``-r`` and ``--reuse-statistics`` prints how often a node is reused in the tree.

.. code-block:: sh

    $ cali-stat -r -o cali-stat-reuse.output 160809-094411_72298_fuu1NeAHT2US.cali 
    == CALIPER: Initialized

The report is the same as above with the following added at the end::

    Reuse statistics:
    Attribute                       #nodes      #elem       #uses       #uses/elem  #uses/node
    cali.attribute.name             38          38          38          1           1           
    cali.attribute.type             8           8           8           1           1           
    cali.attribute.prop             12          7           12          1.71429     1           
    cali.caliper.version            1           1           46          46          46          
    cali.snapshot.event.attr.level  3           3           48          16          16          
    cali.snapshot.event.begin       4           2           17          8.5         4.25        
    cali.snapshot.event.end         4           3           17          5.66667     4.25        
    cali.snapshot.event.set         5           5           24          4.8         4.8         
    event.begin#main                2           1           4           4           2           
    event.end#main                  1           1           2           2           2           
    event.set#main                  3           3           6           2           2           
    main                            5           4           91          22.75       18.2        
    event.end#iteration             1           1           2           2           2           
    event.set#iteration             5           5           10          2           2           
    iteration                       5           5           43          8.6         8.6         
    event.begin#factorial           2           1           13          13          6.5         
    event.end#factorial             2           1           13          13          6.5         
    event.set#factorial             1           1           12          12          12          
    factorial                       22          2           74          37          3.36364

Example Files
--------------------------------

.. _`example.cpp`:

.. literalinclude:: examples/example.cpp
   :language: cpp
