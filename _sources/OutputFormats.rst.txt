Output formatting
================================

Caliper provides a variety of formatters to create machine or
human-readable output. These can be used in all I/O services and tools
that process CalQL, e.g. `cali-query`, the `report` and `mpireport`
services, as well as the report generation API.

Formatter configuration
--------------------------------

To select a formatter, use the `FORMAT` statement in CalQL. Some
formatters, e.g. `tree`, take additional arguments in a "function
call" syntax, e.g. ::

    FORMAT tree(source.function#callpath.address)

The `SELECT` CalQL statement determines the attributes that will be
written by the formatter (all other attributes will be ignored). If
applicable, attributes will be printed in the order they are listed in
the `SELECT` statement.

Some formatters (e.g., `table`) support the `ORDER BY` CalQL statement
to print records in a given order, but not all of them do.

Cali
--------------------------------

The `cali` formatter writes Caliper raw data that can be read by
`cali-query`.

Expand
--------------------------------

The `expand` formatter writes records as comma-separated key-value
lists, with each record in a single line. Hierachical values are
written with slash ``/`` separators.

Example::

    SELECT event.end#function,count(),sum(time.inclusive.duration) GROUP BY event.end#function FORMAT expand

Output::

    event.end#function=TimeIncrement,count=100,time.inclusive.duration=1280
    event.end#function=IntegrateStressForElems,count=100,time.inclusive.duration=358512
    event.end#function=CalcFBHourglassForceForElems,count=100,time.inclusive.duration=700886
    event.end#function=CalcHourglassControlForElems,count=100,time.inclusive.duration=1073630
    event.end#function=CalcVolumeForceForElems,count=100,time.inclusive.duration=1456613
    event.end#function=CalcForceForNodes,count=100,time.inclusive.duration=1466051
    event.end#function=LagrangeNodal,count=100,time.inclusive.duration=1508828

Table
--------------------------------
    
The `table` formatter prints a human-readable text table with
automatically determined column widths. It supports `ORDER BY`.

Example::

    SELECT event.end#function,count(),sum(time.inclusive.duration) GROUP BY event.end#function FORMAT table ORDER BY time.inclusive.duration DESC

Output::

    event.end#function              count time.inclusive.duration
    main                                1                 3395643
    LagrangeLeapFrog                  100                 3379753
    LagrangeElements                  100                 1755059
    LagrangeNodal                     100                 1508828
    CalcForceForNodes                 100                 1466051
    CalcVolumeForceForElems           100                 1456613
    ApplyMaterialPropertiesForElems   100                 1163596
    EvalEOSForElems                  1100                 1113417
    CalcHourglassControlForElems      100                 1073630
    CalcEnergyForElems               3500                  805605
    CalcFBHourglassForceForElems      100                  700886
    IntegrateStressForElems           100                  358512
    CalcPressureForElems            10500                  324182
    CalcQForElems                     100                  309658
    CalcLagrangeElements              100                  266492
    CalcKinematicsForElems            100                  241132
    CalcMonotonicQGradientsForElems   100                  149492
    CalcTimeConstraintsForElems       100                  110236
    CalcMonotonicQRegionForElems     1100                   98944
    CalcCourantConstraintForElems    1100                   49281
    CalcHydroConstraintForElems      1100                   33125
    CalcSoundSpeedForElems           1100                   27975
    UpdateVolumesForElems             100                    8147
    TimeIncrement                     100                    1280


Tree
--------------------------------

The `tree` formatter prints records in hierarchical form based on the
input data, such as nested functions and regions. 

The attribute used to build the hierarchy can be given as an argument
to the formatter (e.g. ``FORMAT tree(function)`` to use `function`). By
default, the hierarchy is built from all attributes flagged as `NESTED`,
which includes the default `function`, `loop`, and `region`
annotations.

Results can be unintuitive if there are multiple records with the same
tree node in the input data. It is best to aggregate data so that
there is only one record for each node.

Example::

    SELECT count(),sum(time.inclusive.duration) WHERE event.end#function GROUP BY function FORMAT tree

Output::

    Path                                       count time.inclusive.duration
    main                                           1                 3395643
      LagrangeLeapFrog                           100                 3379753
        CalcTimeConstraintsForElems              100                  110236
          CalcHydroConstraintForElems           1100                   33125
          CalcCourantConstraintForElems         1100                   49281
        LagrangeElements                         100                 1755059
          UpdateVolumesForElems                  100                    8147
          ApplyMaterialPropertiesForElems        100                 1163596
            EvalEOSForElems                     1100                 1113417
              CalcSoundSpeedForElems            1100                   27975
              CalcEnergyForElems                3500                  805605
                CalcPressureForElems           10500                  324182
          CalcQForElems                          100                  309658
            CalcMonotonicQRegionForElems        1100                   98944
            CalcMonotonicQGradientsForElems      100                  149492
          CalcLagrangeElements                   100                  266492
            CalcKinematicsForElems               100                  241132
        LagrangeNodal                            100                 1508828
          CalcForceForNodes                      100                 1466051
            CalcVolumeForceForElems              100                 1456613
              CalcHourglassControlForElems       100                 1073630
                CalcFBHourglassForceForElems     100                  700886
              IntegrateStressForElems            100                  358512
      TimeIncrement                              100                    1280

.. _json-format:

Json
--------------------------------

The `json` formatter writes a list-of-dicts style json file, where
each Caliper record is a JSON object with attribute:value
entries. Hierarchical values are written as a single string with a
slash (``/``) as separator. Note that the `json` formatter usually
produces larger files than `json-split`.

Optional arguments:

pretty
  More human-readable output (inserts extra whitespace and line breaks).

split
  Just write the records, skip the outer "[ ]" array delimiters.

quote-all
  Write all values as strings using ``"`` quotes, even numerical ones.

Example::

    SELECT function,loop,count(),sum(time.inclusive.duration) WHERE event.end#function GROUP BY function,loop FORMAT json

Output::

    [
    {"function":"main","count":1,"time.inclusive.duration":3395643},
    {"loop":"lulesh.cycle","function":"main/TimeIncrement","count":100,"time.inclusive.duration":1280},
    {"loop":"lulesh.cycle","function":"main/LagrangeLeapFrog","count":100,"time.inclusive.duration":3379753},
    {"loop":"lulesh.cycle","function":"main/LagrangeLeapFrog/LagrangeNodal","count":100,"time.inclusive.duration":1508828},
    {"loop":"lulesh.cycle","function":"main/LagrangeLeapFrog/LagrangeNodal/CalcForceForNodes","count":100,"time.inclusive.duration":1466051},
    {"loop":"lulesh.cycle","function":"main/LagrangeLeapFrog/LagrangeNodal/CalcForceForNodes/CalcVolumeForceForElems","count":100,"time.inclusive.duration":1456613},

.. _json-split-format:

Json-split
--------------------------------

The `json-split` formatter writes a JSON file with separate fields for
Caliper records and metadata. It is generally a more efficient format
than the list-of-dicts produced by the `json` formatter, and
explicitly retains hierarchical information. Attributes flagged as
`NESTED` (e.g., default annotations such as `annotation`, `function`,
and `loop`) will be merged into the `path` column to retain
hierarchical information between them.

The top-level object contains the following fields:

data 
  The Caliper records as a 2D array (array of arrays). Each row is a
  record, each column is an attribute. The attribute/column labels are
  stored in the `columns` array.  Missing values in a record are
  written as `null` entries.

  A column is either a *value* or *reference* entry. Value entries are
  stored directly in each record, whereas reference entries contain the
  index of the corresponding generalized context tree node in the
  `nodes` array. The `is_value` entry in the `column_metadata` array
  determines which columns are value and which are reference entries.

columns
  The labels (i.e., attribute names) of the data columns as array of 
  strings.

column_metadata
  Any metadata for the data columns as array of objects. The objects
  contain the `is_value` entry, which determines if the corresponding 
  data column stores a value or a reference entry.

nodes
  An array of metadata node objects. A reference entry in a data
  record refers to the node at the corresponding index in this array.

  The metadata nodes generally form a tree (or forest), retaining
  hierarchical information. Nodes have a `label` entry with the nodes'
  value, and an optional `parent` entry, which is the index of the
  node's hierarchical parent node in the `nodes` array. It is
  guaranteed that a parent node is placed before all of its children
  in the array.
  
Example::

    SELECT function,loop,count(),sum(time.inclusive.duration) GROUP BY function,loop FORMAT json-split

Output::

    {
       "data": [
        [ 1, 3395643, 0 ],
        [ 100, 1280, 2 ],
        [ 100, 3379753, 3 ],
        [ 100, 1508828, 4 ],
        [ 100, 1466051, 5 ],
        [ 100, 1456613, 6 ],
        [ 100, 358512, 7 ],
        [ 100, 1073630, 8 ],
        [ 100, 700886, 9 ],
        [ 100, 1755059, 10 ],
        [ 100, 266492, 11 ],
        [ 100, 241132, 12 ],
        [ 100, 309658, 13 ],
        [ 100, 149492, 14 ],
        [ 1100, 98944, 15 ],
        [ 100, 1163596, 16 ],
        [ 1100, 1113417, 17 ],
        [ 3500, 805605, 18 ],
        [ 10500, 324182, 19 ],
        [ 1100, 27975, 20 ],
        [ 100, 8147, 21 ],
        [ 100, 110236, 22 ],
        [ 1100, 49281, 23 ],
        [ 1100, 33125, 24 ]
      ],
      "columns": [ "count", "time.inclusive.duration", "path" ],
      "column_metadata": [ { "is_value": true }, { "is_value": true }, { "is_value": false }  ],
      "nodes": [ { "label": "main" }, { "label": "lulesh.cycle", "parent": 0 }, { "label": "TimeIncrement", "parent": 1 }, { "label": "LagrangeLeapFrog", "parent": 1 }, { "label": "LagrangeNodal", "parent": 3 }, { "label": "CalcForceForNodes", "parent": 4 }, { "label": "CalcVolumeForceForElems", "parent": 5 }, { "label": "IntegrateStressForElems", "parent": 6 }, { "label": "CalcHourglassControlForElems", "parent": 6 }, { "label": "CalcFBHourglassForceForElems", "parent": 8 }, { "label": "LagrangeElements", "parent": 3 }, { "label": "CalcLagrangeElements", "parent": 10 }, { "label": "CalcKinematicsForElems", "parent": 11 }, { "label": "CalcQForElems", "parent": 10 }, { "label": "CalcMonotonicQGradientsForElems", "parent": 13 }, { "label": "CalcMonotonicQRegionForElems", "parent": 13 }, { "label": "ApplyMaterialPropertiesForElems", "parent": 10 }, { "label": "EvalEOSForElems", "parent": 16 }, { "label": "CalcEnergyForElems", "parent": 17 }, { "label": "CalcPressureForElems", "parent": 18 }, { "label": "CalcSoundSpeedForElems", "parent": 17 }, { "label": "UpdateVolumesForElems", "parent": 10 }, { "label": "CalcTimeConstraintsForElems", "parent": 3 }, { "label": "CalcCourantConstraintForElems", "parent": 22 }, { "label": "CalcHydroConstraintForElems", "parent": 22 } ]
    }
