Python reader API
===============================================

The caliper-reader is a Python API for reading Caliper's .cali file 
format. You can install it via pip::

    $ pip install caliper-reader

or add the ``python/caliper-reader`` directory in the cloned Caliper
repository to ``PYTHONPATH``.

Usage
---------------------------------------

The :py:class:`caliperreader.CaliperReader` class reads a Caliper file and then provides its
contents in the `records` and `globals` class members, where `records`
is a Python list-of-dicts containing the recorded performance data
and `globals` is a Python dict with program metadata about the run.
The dicts represent Caliper attribute:value records: the key is the
Caliper attribute name; the value is a string or list of strings.
The example below prints the `avg#inclusive#sum#time.duration` metric
for every region path in the provided example profile data file::

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

The CaliperReader `attributes()` function returns the list of Caliper
attributes. The `attribute()` function returns an `Attribute` object
to query metadata for a given Caliper attribute name::

    a = r.attribute('avg#inclusive#sum#time.duration')
    a.get('attribute.unit')
    'sec'

You can use the :py:func:`caliperreader.read_caliper_contents` function as a
shortcut to read Caliper data without creating a `CaliperReader` object::

    (records,globals) = cr.read_caliper_contents('example-profile.cali')

API reference
------------------------------------------

.. automodule:: caliperreader
    :members: read_caliper_contents, read_caliper_globals

.. autoclass:: caliperreader.CaliperReader
    :members:

.. autoclass:: caliperreader.CaliperStreamReader
    :members:

.. autoclass:: caliperreader.metadatadb.Attribute
    :members: