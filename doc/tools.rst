Caliper Tools
================================

Caliper comes with additional tool programs for post-processing
the raw Caliper output data (in ``.cali`` files by default).

Caliper’s output is in the form of snapshots. Snapshots
hold the information chosen to be recorded by the enabled services.
See :doc:`services` for more.


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
// -s example //
// --print-attributes example //
// -a example //
// -o example //
// example of multiple file selection //

// example of non-expand vs expand? //

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
(forthcoming)

Cali-stat
--------------------------------

Collect and print statistics about data elements in Caliper streams.
Currently an internal debugging tool for the Caliper context tree.

Usage
````````````````````````````````
``cali-stat [OPTIONS]... [FILE]...``

Options
````````````````````````````````
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-r`` | ``--reuse-statistics``            | Prints statistics about the reuse of the branches of the snapshot   |
|        |                                   | record tree. More reuse is more efficient.                          |
+--------+-----------------------------------+---------------------------------------------------------------------+
| ``-h`` | ``--help``                        | Print the help message, a summary of these options.                 |
+--------+-----------------------------------+---------------------------------------------------------------------+

Files
````````````````````````````````
The files used by ``cali-stat`` are ``.cali`` record files produced from running Caliper
in a program. Only one ``.cali`` file can be read at a time.

Examples
````````````````````````````````
(forthcoming)

Cali-graph
--------------------------------

Calling the ``cali-graph`` function on a ``.cali`` files will export
the general context tree in a graphviz (``.dot``) format.

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
(forthcoming)
