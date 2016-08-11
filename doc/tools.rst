Caliper Tools
================================

Caliper comes with additional tool programs for post-processing
the raw Caliper output data (in ``.cali`` files by default).

Caliper’s output is in the form of snapshots. Snapshots
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
| ``-s`` | ``--select=QUERY_STRING``         | Selects snapshots to expand. Snapshots can be filtered by which     |
|        |                                   | attributes or variables appear, and specific states of attributes or|
|        |                                   | variables. When this is set, all other snapshots are ignored in the |
|        |                                   | ``expand`` command. Snapshots with specific attributes and variables|
|        |                                   | or values thereof can be excluded by using a ``-`` symbol in front  |
|        |                                   | of the name of the attribute/variable. Specific attribute or        |
|        |                                   | variable values are listed in ``attribute#value`` format. Multiple  |
|        |                                   | attributes/variables are selected by listing with a ``:`` separator.|
|        |                                   | The default behavior is to select all snapshots.                    |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-e`` | ``--expand``                      | Expands the selected snapshots (from ``-s``) and prints the selected|
|        |                                   | attributes (from ``--print-attributes``). Default behavior is to    |
|        |                                   | expand and print all snapshots and attributes.                      |              
+--------+-----------------------------------+---------------------------------------------------------------------+
|        | ``--print-attributes=ATTRIBUTES`` | Select which attributes to print when expanded. Attributes can be   |
|        |                                   | excluded by using a ``-`` symbol in front of the name. Multiple     |
|        |                                   | attributes are selected/excluded by listing with a ``:`` separator. |
|        |                                   | By default, all attributes are printed.                             |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-a`` | ``--aggregate=ATTRIBUTES``        | Aggregate the specified attributes. Currently, aggregation only     |
|        |                                   | occurs over matching snapshots.                                     |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-o`` | ``--output=FILE``                 | Set the name of the output file.                                    |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-h`` | ``--help``                        | Print the help message, a summary of these options.                 |
+--------+-----------------------------------+---------------------------------------------------------------------+

Files
````````````````````````````````
The files used by ``cali-query`` are ``.cali`` record files produced from running Caliper
in a program. Multiple ``.cali`` files can be read at once.

Examples
````````````````````````````````
``cali-query`` can only be used on the ``.cali`` files created by using a configured
Caliper library in a program. The program example.cpp_ was run:

.. code-block:: sh

    $ CALI_SERVICES_ENABLE=event:recorder:timestamp:trace example
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

For comparison, here are the first six lines of records from this file::

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

The first six lines of records after processing look like this::

    __rec=node,id=8,attr=8,data=cali.attribute.name,parent=3
    __rec=node,id=9,attr=8,data=cali.attribute.type,parent=7
    __rec=node,id=10,attr=8,data=cali.attribute.prop,parent=1
    __rec=node,id=11,attr=10,data=12,parent=3
    __rec=node,id=12,attr=8,data=cali.caliper.version,parent=11
    __rec=node,id=13,attr=10,data=148,parent=0

Without using ``-e`` or ``--expand``, ``cali-query`` does not change the format
of the records.

``-e`` and ``--expand``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The ``-e`` or ``--expand`` option expands the records in to a more readable form.

.. code-block:: sh

    $ cali-query -e -o cali-query-expanded.output 160809-094411_72298_fuu1NeAHT2US.cali
    == CALIPER: Initialized

The first six lines of records after expansion look like this::

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

    $ cali-query -s "iteration=3:factorial" -e -o cali-query-selected.output 160809-094411_72298_fuu1NeAHT2US.cali
    == CALIPER: Initialized

Here, only the records that contain the ``iteration=3`` attribute value and the ``factorial`` attribute
are expanded and written. These are the first six lines of records written::

    event.set#factorial=comp,cali.snapshot.event.set=70,cali.snapshot.event.attr.level=1,factorial=init,iteration=3,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=15
    event.begin#factorial=init,cali.snapshot.event.begin=70,cali.snapshot.event.attr.level=2,factorial=comp,iteration=3,main=body/loop,cali.caliper.version=1.5.dev
    event.set#factorial=comp,cali.snapshot.event.set=70,cali.snapshot.event.attr.level=1,factorial=comp/init,iteration=3,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=36
    event.begin#factorial=init,cali.snapshot.event.begin=70,cali.snapshot.event.attr.level=2,factorial=comp/comp,iteration=3,main=body/loop,cali.caliper.version=1.5.dev
    event.set#factorial=comp,cali.snapshot.event.set=70,cali.snapshot.event.attr.level=1,factorial=comp/comp/init,iteration=3,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=37
    event.end#factorial=comp,cali.snapshot.event.end=70,cali.snapshot.event.attr.level=1,factorial=comp/comp/comp,iteration=3,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=14


``-a`` and ``--aggregate``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
``-a`` and ``--aggregate`` add up the values of the specified attribute(s) on any repetition
of the set of values of the other attributes.

.. code-block:: sh

    $ cali-query -a "time.inclusive.duration" -e -o cali-query-aggregated.output 160809-094411_72298_fuu1NeAHT2US.cali
    == CALIPER: Initialized

This is the last six records after aggregation of the ``time.inclusive.duration`` attribute::

    event.end#factorial=comp,cali.snapshot.event.end=70,cali.snapshot.event.attr.level=1,factorial=comp/comp/comp/comp,iteration=4,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=14
    event.set#iteration=2,cali.snapshot.event.set=60,cali.snapshot.event.attr.level=1,iteration=1,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=69
    event.set#iteration=3,cali.snapshot.event.set=60,cali.snapshot.event.attr.level=1,iteration=2,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=183
    event.set#iteration=4,cali.snapshot.event.set=60,cali.snapshot.event.attr.level=1,iteration=3,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=164
    event.end#iteration=4,cali.snapshot.event.end=60,cali.snapshot.event.attr.level=1,iteration=4,main=body/loop,cali.caliper.version=1.5.dev,time.inclusive.duration=210
    event.end#main=conclusion,cali.snapshot.event.end=38,cali.snapshot.event.attr.level=1,main=body/conclusion,cali.caliper.version=1.5.dev,time.inclusive.duration=37


``--print-attributes``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
``--print-attributes`` controls which parts of each record are written to the output.
Only the selected attributes will print in each record.

.. code-block:: sh

    $ cali-query -a "time.inclusive.duration" --print-attributes="main:iteration:time.inclusive.duration" -e -o cali-query-print-aggregated.output 160809-094411_72298_fuu1NeAHT2US.cali
    == CALIPER: Initialized

This is the last six records as above, but printing only the
``main``, ``iteration``, and ``time.inclusive.duration`` attributes::

    iteration=4,main=body/loop,time.inclusive.duration=14
    iteration=1,main=body/loop,time.inclusive.duration=69
    iteration=2,main=body/loop,time.inclusive.duration=183
    iteration=3,main=body/loop,time.inclusive.duration=164
    iteration=4,main=body/loop,time.inclusive.duration=210
    main=body/conclusion,time.inclusive.duration=37



Cali-print
--------------------------------

Print expanded records from ``cali-query`` in a table format.

Usage
````````````````````````````````
``cali-print [OPTIONS]... [FILE]...``

Options
````````````````````````````````
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-f`` | ``--format=FORMAT_STRING``        | Print the entries of the table in the format specified by           |
|        |                                   | ``FORMAT_STRING``. ``FORMAT_STRING`` should be of the form:         |
|        |                                   | ``%[width1]attr1% %[width2]attr2% ...``, where ``width`` is the     |
|        |                                   | minimum width in characters of the printed value for the matching   |
|        |                                   | attribute, ``attr``. Limits the printed attributes to those         |
|        |                                   | listed in ``FORMAT_STRING``.                                        |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-s`` | ``--select=QUERY_STRING``         | Select the attributes to print in the table when not specifying the |
|        |                                   | ``FORMAT_STRING`` with ``--format``. ``QUERY_STRING`` should be of  |
|        |                                   | the form: ``attr1:attr2: ...``.                                     |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-t`` | ``--title=STRING``                | Specify a custom title or header line.                              |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-o`` | ``--output=FILE``                 | Set the name of the output file.                                    |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-h`` | ``--help``                        | Print the help message, a summary of these options.                 |
+--------+-----------------------------------+---------------------------------------------------------------------+


Files
````````````````````````````````
The files used by ``cali-print`` are expanded records produced by ``cali-query``. Only
one file may be read at a time.


Examples
````````````````````````````````
All of these examples are using as input output files from the ``cali-query`` examples
above. ``cali-print`` can only read ``cali-query`` outputs that have used the ``-e`` or
``--expand`` option.

Basic Use
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
``cali-print`` will, if nothing is specified, automatically format and print
all attributes that are not internal Caliper information or event triggers.
Note that all examples will use the ``-o`` option to specify output file.

.. code-block:: sh

    $ cali-print -o cali-print.output cali-query-expanded.output

The first eight lines of records and title line appear as so::

    factorial                 iteration                 main                      time.inclusive.duration    
                                                                                                             
                                                        init                      1813                       
                                                        body                                                 
                                                        body/init                 114                        
                                                        body/loop                                            
                              0                         body/loop                                            
    init                      0                         body/loop                 215                        
    comp                      0                         body/loop                 21                         
                              0                         body/loop                 529

The same can be done with the aggregated output:

.. code-block:: sh

    $ cali-print -o cali-print-agg.output cali-query-aggregated.output

And the results are similar::

    factorial                 iteration                 main                      time.inclusive.duration    
                                                        init                      1813                       
                                                        body/init                 114                        
                                                        body/loop                 1214                       
                              0                         body/loop                 529                        
    init                      0                         body/loop                 215                        
    init                      1                         body/loop                 15                         
    init                      2                         body/loop                 15                         
    comp/init                 2                         body/loop                 101


``-f`` and ``--format``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
``-f`` and ``--format`` can be used to specify a specific output format
for the table, including limiting the printed attributes. Use the form
``%[width1]attr1% %[width2]attr2% ...``.
.. code-block:: sh

    $ cali-print -f "%[10]main% %[20]factorial% %[10]time.inclusive.duration%" -o cali-print-agg-format.output cali-query-aggregated.output

The title line and first eight records are formatted as specified::

    main         factorial              time.inclusive.duration
    init                                1813      
    body/init                           114       
    body/loop                           1214      
    body/loop                           529       
    body/loop    init                   215       
    body/loop    init                   15        
    body/loop    init                   15        
    body/loop    comp/init              101


``-s`` and ``--select``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
``-s`` and ``--select`` will choose which attributes to print, but let ``cali-print``
autoformat the table. Use the form ``attr1:attr2:...``.
.. code-block:: sh

    $ cali-print -s "main:factorial:time.inclusive.duration" -o cali-print-agg-select.output cali-query-aggregated.output

The title line and first eight records::

    main                      factorial                 time.inclusive.duration    
    init                                                1813                       
    body/init                                           114                        
    body/loop                                           1214                       
    body/loop                                           529                        
    body/loop                 init                      215                        
    body/loop                 init                      15                         
    body/loop                 init                      15                         
    body/loop                 comp/init                 101


``-t`` and ``--title``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
For a custom title string for the table, use ``-t`` or ``--title``. The format is any string.
.. code-block:: sh

    $ cali-print -f "%[10]main% %[20]factorial% %[10]time.inclusive.duration%" -t "Main         Factorial              Time" -o cali-print-agg-format-title.output cali-query-aggregated.output

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
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
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


Cali-graph
--------------------------------

Calling the ``cali-graph`` function on a ``.cali`` files will export
the general context tree in a graphviz (``.dot``) format. Use the 
``-o`` or ``--output`` option to specify a ``.dot`` output file.

Usage
````````````````````````````````
``cali-graph [OPTIONS]... [FILES]...``

Options
````````````````````````````````
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-n`` | ``--max-nodes=NUMBER_OF_NODES``   | This option limits the number of nodes exported into the graph to   |
|        |                                   | the first NUMBER_OF_NODES. Nodes are the individual attribute values|
|        |                                   | numbered as encountered in the course of the program. An attribute  |
|        |                                   | with the same name and same value but different parent node is      |
|        |                                   | considered a unique node.                                           |
+--------+-----------------------------------+---------------------------------------------------------------------+
|        | ``--skip-attribute-prefixes``     | If called, the graph does not print the name of the attribute for   |
|        |                                   | each node, only the value of the attribute.                         |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-o`` | ``--output=FILE``                 | Set the name of the output file.                                    |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-h`` | ``--help``                        | Print the help message, a summary of these options.                 |
+--------+-----------------------------------+---------------------------------------------------------------------+

Files
````````````````````````````````
The files used by ``cali-graph`` are ``.cali`` record files produced from running Caliper
in a program. Multiple ``.cali`` files can be read at once.

Examples
````````````````````````````````

Basic Use
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
.. code-block:: sh

    $ cali-graph -o cali-graph.output 160809-094411_72298_fuu1NeAHT2US.cali 
    == CALIPER: Initialized

This is a sample from a text only output:: 

    1 -- 15;
    16 [label="cali.attribute.name:cali.lvl.22"];
    15 -- 16;
    17 [label="cali.attribute.name:cali.lvl.38"];
    15 -- 17;
    18 [label="cali.attribute.prop:84"];
    2 -- 18;
    19 [label="cali.attribute.name:cali.snapshot.event.attr.level"];
    18 -- 19;
    20 [label="cali.attribute.name:cali.snapshot.event.begin"];
    18 -- 20;
    21 [label="cali.attribute.name:cali.snapshot.event.end"];


``-n`` and ``--max-nodes``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
``-n`` and ``--max-nodes`` limit the number of nodes graphed to the first
``NUMBER_OF_NODES``, an ``int``.

.. code-block:: sh

    $ cali-graph -n 16 -o cali-graph-max-nodes.output 160809-094411_72298_fuu1NeAHT2US.cali 
    == CALIPER: Initialized

This is the entire file as printed::

    graph {
      0 [label=":usr"];
      1 [label=":int"];
      2 [label=":uint"];
      3 [label=":string"];
      4 [label=":addr"];
      5 [label=":double"];
      6 [label=":bool"];
      7 [label=":type"];
      8 [label=":cali.attribute.name"];
      3 -- 8;
      9 [label="cali.attribute.name:cali.attribute.type"];
      7 -- 9;
      10 [label="cali.attribute.name:cali.attribute.prop"];
      1 -- 10;
      11 [label="cali.attribute.prop:12"];
      3 -- 11;
      12 [label="cali.attribute.name:cali.caliper.version"];
      11 -- 12;
      13 [label="cali.attribute.prop:148"];
      0 -- 13;
      14 [label="cali.attribute.name:cali.key.attribute"];
      13 -- 14;
      15 [label="cali.attribute.prop:213"];
      1 -- 15;
    }


``--skip-attribute-prefixes``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
``--skip-attribute-prefixes`` shortens the names of each node.

.. code-block:: sh

    $ cali-graph --skip-attribute-prefixes -o cali-graph-skip.output 160809-094411_72298_fuu1NeAHT2US.cali 
    == CALIPER: Initialized

The prefix of the node name is suppressed::

    1 -- 15;
    16 [label="name:cali.lvl.22"];
    15 -- 16;
    17 [label="name:cali.lvl.38"];
    15 -- 17;
    18 [label="prop:84"];
    2 -- 18;
    19 [label="name:cali.snapshot.event.attr.level"];
    18 -- 19;
    20 [label="name:cali.snapshot.event.begin"];
    18 -- 20;
    21 [label="name:cali.snapshot.event.end"];


Example Files
--------------------------------

.. _`example.cpp`:
example.cpp
.. literalinclude:: examples/example.cpp
   :language: cpp
.. _`.cali file`:
160809-094411_72298_fuu1NeAHT2US.cali
.. literalinclude:: examples/160809-094411_72298_fuu1NeAHT2US.cali
   :language: text
.. _`cali-query.output`:
cali-query.output
.. literalinclude:: examples/cali-query.output
   :language: text
.. _`cali-query-expanded.output`:
cali-query-expanded.output
.. literalinclude:: examples/cali-query-expanded.output
   :language: text
.. _`cali-query-selected.output`:
cali-query-selected.output
.. literalinclude:: examples/cali-query-selected.output
   :language: text
.. _`cali-query-aggregated.output`:
cali-query-aggregated.output

cali-query-print-aggregated.output

cali-print.output

cali-print-agg.output

cali-print-agg-format.output

cali-print-agg-select.output

cali-print-agg-format-title.output

cali-stat.output

cali-stat-reuse.output

cali-graph.output

cali-graph-max-nodes.output

cali-graph-skip.output
