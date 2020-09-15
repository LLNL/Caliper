# Copyright (c) 2020, Lawrence Livermore National Security, LLC.
# See top-level LICENSE file for details.
#
# SPDX-License-Identifier: BSD-3-Clause

from .metadatadb import Attribute, Node, MetadataDB

class ReaderError(Exception):
    def __init__(self, message):
        self.message = message


class CaliperReader:
    """ Reads a Caliper .cali file and exports its contents.

    Use the read() method to read a Caliper .cali file. Contents are then
    available in the "records" and "globals" attributes. Each CaliperReader
    object should read a single file.

    Attributes:
        records : list-of-dicts
            The snapshot records read from the .cali file.
        globals : dict
            The global key:value attributes defined in the .cali file
    """

    def __init__(self):
        self.db = MetadataDB()

        self.records = []
        self.globals = {}

        self.num_records = {}


    def read(self, filename_or_stream):
        """ Read a .cali file or stream.

        After reading, the file's contents are available in the CaliperReader
        object's "records" and "globals" attributes.

        Arguments:
            filename_or_stream : str or stream object
                The filename or stream to read.
        """

        if isinstance(filename_or_stream, str):
            with open(filename_or_stream) as f:
                for line in f:
                    self._process(line)
        else:
            for line in filename_or_stream:
                self._process(line)


    def attribute_list(self):
        """ Return the attribute keys.

        A file must have been read with read().

        Returns:
            The attribute keys (list of strings).
        """
        return self.db.attributes.keys()


    def attribute(self, attribute_name):
        """ Return a Caliper Attribute object for the given attribute name.
        """

        return self.db.attributes[attribute_name]


    def _process(self, line):
        record = _read_cali_record(line)

        if not '__rec' in record:
            raise ReaderError('"__rec" missing: ' + line)

        kind = record['__rec'][0]

        if kind == 'node':
            self._process_node_record(record)
        elif kind == 'ctx':
            self._process_ctx_record(record)
        elif kind == 'globals':
            self._process_globals_record(record)

        if not kind in self.num_records:
            self.num_records[kind] = 0

        self.num_records[kind] += 1


    def _process_node_record(self, record):
        parent = Node.CALI_INV_ID

        if 'parent' in record:
            parent = int(record['parent'][0])

        self.db.import_node(int(record['id'][0]), int(record['attr'][0]), record['data'][0], parent)


    def _process_ctx_record(self, record):
        self.records.append(self._expand_record(record))


    def _process_globals_record(self, record):
        self.globals = self._expand_record(record)


    def _expand_record(self, record):
        result = {}

        if 'ref' in record:
            for node_id in record['ref']:
                for k, v in self._expand_node(int(node_id)).items():
                    if len(v) > 1:
                        result[k] = v
                    else:
                        result[k] = v[0]

        if 'attr' in record and 'data' in record:
            for attr_id, val in zip(record['attr'], record['data']):
                result[self.db.nodes[int(attr_id)].data] = val

        return result


    def _expand_node(self, node_id):
        def add(result, key, data):
            if key not in result:
                result[key] = []
            result[key].insert(0, data)

        result = {}
        node = self.db.nodes[node_id]

        while node is not None:
            attr = node.attribute()

            add(result, attr.name(), node.data)
            if attr.is_nested():
                add(result, 'path', node.data)

            node = node.parent

        return result


def _split_escape_string(input, keep_escape = True, splitchar = ',', escapechar = '\\'):
    """ Splits a string but allows escape characters """

    results = []
    string  = ""
    escaped = False

    for c in input:
        if not escaped and c == escapechar:
            escaped = True
            if keep_escape:
                string += c
        elif not escaped and c == splitchar:
            results.append(string)
            string = ""
        else:
            string += c
            escaped = False

    results.append(string)

    return results


def _read_cali_record(line):
    record  = {}
    entries = _split_escape_string(line.strip())

    for e in entries:
        kv = _split_escape_string(e, False, '=')

        if len(kv) < 1:
            raise ReaderError('Invalid record: {}'.format(line))

        record[kv[0]] = kv[1:]

    return record


def read_caliper_contents(filename_or_stream):
    """ Reads a Caliper .cali file and returns its contents.

    Arguments:
        filename_or_stream:
            Stream object or the filename of the .cali file.

    Returns:
        (records, globals):
            A tuple with the Caliper snapshot records and globals. The first
            element contains the snapshot record as a list of dicts. The
            second element contains the global key:value attributes as a
            dict.
    """

    reader = CaliperReader()
    reader.read(filename_or_stream)

    return (reader.records, reader.globals)


def read_caliper_globals(filename_or_stream):
    """ Reads a Caliper .cali file and returns its global attributes.

    Arguments:
        filename_or_stream:
            Stream object or the filename of the .cali file.

    Returns:
        dict:
            The global key:value attributes as a dict.
    """

    reader = CaliperReader()
    reader.read(filename_or_stream)

    return reader.globals