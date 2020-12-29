The CalQL query language
================================

The Caliper Query Language (CalQL) is used to filter, aggregate, and
create reports from Caliper data in Caliper's data processing tools
(cali-query) and report services. Its syntax is very similar to SQL.

For example, filtering and aggregating a trace into a function time
profile for an application's main loop and printing results in a
table, sorted by time, is accomplished with the following statement:
::

  SELECT *, count(), sum(time.duration)
  WHERE loop=mainloop
  GROUP BY function
  FORMAT table
  ORDER BY time.duration

Quick reference
--------------------------------

This table contains a quick reference of all CalQL statements:

::

  LET <list>                   # Compute additional attributes in input records
    <a>=ratio(<b>,<c>,<S>)     # computes a = <b>/<c>*S
    <a>=scale(<b>,<S>)         # computes <a> = <b>*S
    <a>=truncate(<b>,<S>)      # computes <a> = <b> - mod(<b>, S)
    <a>=first(<a0>,<a1>, ...)  # <a> is the first of <a0>, <a1>, ... found in the input record
    ... IF <condition>         # apply only if input record meets condition

  SELECT <list>                # Select attributes and define aggregations (i.e., select columns)
    *                          # select all attributes
    <attribute>                # select <attribute>
    count()                    # number of input records in grouping
    sum(<attribute>)           # compute sum of <attribute> for grouping
    min(<attribute>)           # compute min of <attribute> for grouping
    max(<attribute>)           # compute max of <attribute> for grouping
    avg(<attribute>)           # compute average of <attribute> for grouping
    percent_total(<attribute>) # compute percent of total sum for <attribute> in grouping
    ratio(<a>,<b>,<S>)         # compute sum(<a>)/sum(<b>)*S for grouping
    inclusive_sum(<a>)         # computes inclusive sum of <a> in a nested hierarchy
    scale(<a>,<S>)             # computes sum of <a> multiplied by scaling factor S
    inclusive_scale(<a>,<S>)   # computes inclusive scaled sum of <a> in a nested hierarchy
    inclusive_percent_total(<a>) # computes inclusive percent-of-total of <a> in a nested hierarchy
    any(<attribute>)           # pick one value for <attribute> out of all records in grouping
    ... AS <name>              # use <name> as column header in tree or table formatter
    ... UNIT <unit>            # use <unit> as unit name

  GROUP BY <list>              # Define aggregation grouping (what to aggregate over, e.g. "function,mpi.rank")
    <attribute>                # include <attribute> in grouping
    prop:nested                # include default annotation attributes ('function', 'annotation', ...) in grouping

  WHERE <list>                 # define filter (i.e., select records/rows)
    <attribute>                # records where <attribute> exists
    <attribute> = <value>      # records where <attribute> = <value>
    <attribute> < <value>      # records where <attribute> > <value>
    <attribute> > <value>      # records where <attribute> < <value>
    NOT <clause>               # negate the filter clause

  FORMAT <formatter>           # Define output format
    cali                       # .cali format
    expand                     # “<attribute1>=<value1>,<attibute2>=<value2>,...”
    json                       # write json records { “attribute1”: “value1”, “attribute2”: “value2” }
      (pretty)                 #   ... in a more human-readable format
      (object)                 #   ... in a json object that includes attribute info and globals
    json-split                 # json format w/ separate node hierarchy for hatchet library
    table                      # human-readable text table
    tree                       # human-readable text tree output

  ORDER BY <list>              # Sort records in output (table formatter only)
    <attribute>                # order by <attribute>
    ... ASC                    # sort in ascending order
    ... DESC                   # sort in descending order

LET
--------------------------------

The LET statement applies computation operators on input record entries
and adds the results as a new entries in the input records. The new entries
can then be used in subsequent SELECT, GROUP BY, or FORMAT statements.
For example, we can use the scale() operator to scale a value before
subsequent aggregations::

  LET
    sec=scale(time.duration,1e-6)
  SELECT
    prop:nested,sum(sec)

We can use the truncate() operator on an iteration counter to
aggregate blocks of 10 iterations in a time-series profile::

  LET
    block=truncate(iteration#mainloop,10)
  SELECT
    block,sum(time.duration)
  GROUP BY
    block

The first() operator returns the first attribute out of a list of attribute
names found in an input record. It can also be used to rename attributes::

  LET
    time=first(time.duration,sum#time.duration)
  SELECT
    sum(time) AS Time
  GROUP BY
    prop:nested

LET terms have the general form

  a = f(...) [IF <condition>]

where f is one of the operators and `a` is the name of the result attribute.
The result is added to the input records before the record is processed further.
Result entries are only added to a record if all required input operands are
present.

With the optional IF condition, the operation is only applied for input records
that meet a condition. One can use this to compute values for a specific
subset of records. The condition clauses use the same syntax as WHERE filter
clauses. The example below defines a "work" attribute with the time in
records that contain "omp.work" regions, and then uses that to compute
efficiency from the total and "work" time:

  LET
    work=first(time.duration) IF omp.work
  SELECT
    sum(time.duration)        AS Total,
    sum(work)                 AS Work,
    ratio(work,time.duration) AS Efficiency
  GROUP BY
    prop:nested

SELECT
--------------------------------

The SELECT statement selects which attributes in snapshot records
should be printed, and defines aggregations. ``SELECT *`` selects all
attributes. ``SELECT op(arg)`` enables aggregation operation `op` with
argument `arg`. Generally, attributes will be printed in the order
given in the SELECT statement.

An example to print all attributes and enable visit count aggregation::

  SELECT *, count()

Aggregation operations create a new output attribute. The name is typically
derived from the input attribute(s). For example, the result of ``sum(attr)``
is stored in ``sum#attr``. All selection attributes and aggregation
arguments must come from the input data; recursive
aggregations (e.g., ``min(count())``) within a query are not supported.

The ``AS`` keyword assigns a user-defined name to a selected attribute
or aggregation result. Example::

  SELECT sum(time.duration) AS "Time (usec)" FORMAT table

Here, the `table` formatter uses "Time (usec)" instead of "sum#time.duration" as
column name for the ``sum(time.duration)`` column. Only some
formatters (table, tree, json, and json-split) support ``AS``.

Inclusive aggregation operations (`inclusive_sum`, `inclusive_scale`, and
`inclusive_percent_total`) compute inclusive values (value for a tree node
plus all of its children) for datasets with hierarchical regions. This
applies to the hierarchy defined by attributes with the
``CALI_ATTR_NESTED`` property, including the default `function`,
`annotation`, and `loop` attributes from Caliper's high-level annotation macros.

A more complex example::

  SELECT
    *,
    scale(time.duration,1e-6) AS Time,
    inclusive_percent_total(time.duration) AS "Time %"
  GROUP BY
    prop:nested
  FORMAT
    tree

The computes the (exclusive) sum of `time.duration` divided by 100000 and the inclusive
percent-of-total for `time.duration`. Example output::

  Path      Time  Time %
  main         5     100
    foo       35      90
      bar     10      20

WHERE
--------------------------------

The WHERE statement can be used to filter the records to aggregate/print.
The statement takes a comma-separated list of clauses. Records that don't
match all of the clauses are filtered out. Filters can only be defined on
input attributes, i.e. it is not possible to filter on aggregation
results.

Currently, there are clauses to test for existance of an attribute
label in a record, and to filter for specific attribute
values. Clauses can be negated with ``NOT``. Example: ::

  WHERE loop,function=foo

Select records that contain ``loop`` attributes where function equals
``foo``. Note: for nested attributes, the match may occur on any
level. The above example matches the following::

  { loop=mainloop, iteration=5, function=main/foo }      Matches
  { loop=mainloop, iteration=5, function=main/foo/bar }  Matches ('function=foo' will be matched on any nesting level)
  { function=main/foo }                                  No match: 'loop' attribute missing

The ``NOT`` keyword negates clauses: ::

  WHERE NOT iteration#mainloop=4

matches every record where ``iteration#mainloop`` is not 4 (including
records without ``iteration#mainloop`` attributes.

GROUP BY
--------------------------------

The GROUP BY statement defines the `aggregation key` for aggregation
operations. The aggregation key defines for which attributes separate
(aggregate) records will be kept. That is, the aggregator will
generate an aggregate record for each unique combination of key values
found in the input records.  The values of the aggregation attributes
in the input records will be accumulated and appended to the aggregate
record.

Consider the following table of input records::

  function loop     iteration
           mainloop
  foo      mainloop         0
  bar      mainloop         0
  foo      mainloop         1
  bar      mainloop         1
  foo      mainloop         2
  bar      mainloop         2


With this input, the following GROUP BY statement will create a
function profile::

  SELECT *, count() GROUP BY function

  function count
  foo          3
  bar          3

FORMAT
--------------------------------

The FORMAT statement selects the output format option. Caliper can
produce machine-readable (e.g., json or Caliper's own csv-style) or
human-readable output (text tables or a tree representation).

See :doc:`OutputFormats` for a list of available formatters.

ORDER BY
--------------------------------

Sort output records by the given sort criteria. The statement defines
a list of attributes to sort output records by. Records can be sorted
ascending (using the ASC keyword) or descending (using DESC). Note
that the sorting is performed by the output formatter and only
available in some formatters (e.g., table).

The following example prints a iteration/function profile ordered by
time and iteration number. Note that one must use the original
attribute name and not an alias assigned with ``AS``: ::

  SELECT
    *,
    sum(time.inclusive.duration) AS Time
  FORMAT
    table
  ORDER BY
    sum#time.inclusive.duration DESC,
    iteration#mainloop

  function loop     iteration#mainloop     Time
  main                                   100000
  main     mainloop                       80000
  main/foo mainloop                  0     2500
  main     mainloop                  0     1500
  main/foo mainloop                  1     3500
  main     mainloop                  1     2000
  main     mainloop                  2     1000
  main/foo mainloop                  2      600
  ...
