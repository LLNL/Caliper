caliper-reader: A Python reader for Caliper files
=================================================

This Python package reads the native .cali files produced by the
[Caliper](https://github.com/LLNL/Caliper) performance profiling
library.

You can install caliper-reader with pip:

    $ pip install caliper-reader

Alternatively, add the `python/caliper-reader` path in the cloned
Caliper repository to `PYTHONPATH`.

Usage
---------------------------------------

The `CaliperReader` class reads a Caliper file and then provides its
contents in the `records` and `globals` class members, where `records`
is a Python list-of-dicts containing the recorded performance data
and `globals` is a Python dict with program metadata about the run.
The dicts represent Caliper attribute:value records: the key is the
Caliper attribute name; the value is a string or list of strings.
The example below prints the `avg#inclusive#sum#time.duration` metric
for every region path in the provided example profile data file:

```Python
import caliperreader as cr

r = cr.CaliperReader()
r.read('example-profile.cali')

metric = 'avg#inclusive#sum#time.duration'

for rec in r.records:
    path = rec['path'] if 'path' in rec else 'UNKNOWN'
    time = rec[metric] if metric in rec else '0'

    if (isinstance(path, list)):
        path = "/".join(path)

    print("{0}: {1}".format(path, time))
```

The CaliperReader `attributes()` function returns the list of Caliper
attributes. The `attribute()` function returns an `Attribute` object
to query metadata for a given Caliper attribute name:

```Python
>>> a = r.attribute('avg#inclusive#sum#time.duration')
>>> a.get('attribute.unit')
'sec'
```

You can use the `read_caliper_contents` function as a shortcut to read
Caliper data without creating a `CaliperReader` object:

```Python
(records,globals) = cr.read_caliper_contents('example-profile.cali')
```

Use `read_caliper_globals` if you only need the global (metadata) record:

```Python
globals = cr.read_caliper_globals('example-profile.cali')
```

Authors
---------------------------------------

Caliper was created by [David Boehme](https://github.com/daboehme), boehme3@llnl.gov.

Release
-------------------------------------

Caliper is released under a BSD 3-clause license.
See [LICENSE](LICENSE) for details.

``LLNL-CODE-678900``