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

Finally, some formatters (e.g., `table`) support the `ORDER BY` CalQL
statement to print records in a given order.

Cali
--------------------------------

The `cali` formatter writes Caliper raw data that can be read by
`cali-query`.

Expand
--------------------------------

The `expand` formatter writes records as comma-separated key-value
lists::

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
automatically determined column widths. It supports `ORDER BY`::

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
