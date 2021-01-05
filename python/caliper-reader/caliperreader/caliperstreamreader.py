# Copyright (c) 2020, Lawrence Livermore National Security, LLC.
# See top-level LICENSE file for details.
#
# SPDX-License-Identifier: BSD-3-Clause

from .metadatadb  import Attribute, Node, MetadataDB
from .readererror import ReaderError

class CaliperStreamReader:
    """ Reads a Caliper .cali data stream

    Use the read() method to read a Caliper .cali file and process
    records using callback methods.

    Attributes:
        globals     : dict
            The global key:value attributes defined in the .cali file
        num_records : dict
            The number of different kinds of records read
    """

    def __init__(self):
        self.db = MetadataDB()
        self.globals = {}
        self.num_records = {}


    def read(self, filename_or_stream, process_record_fn = None):
        """ Read a .cali file or stream.

        Snapshot records being read are fed to the user-provided
        process_record_fn callback function.
        After reading, global (metadata) values are stored in the
        globals attribute.

        Arguments:
            filename_or_stream : str or stream object
                The filename or stream to read.
            process_record_fn  : Function accepting a dict
                A callback function to process performance data records
        """

        if isinstance(filename_or_stream, str):
            with open(filename_or_stream) as f:
                for line in f:
                    self._process(line, process_record_fn)
        else:
            for line in filename_or_stream:
                self._process(line, process_record_fn)


    def attributes(self):
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


    def _process(self, line, process_record_fn = None):
        record = _read_cali_record(line)

        if not '__rec' in record:
            raise ReaderError('"__rec" missing: ' + line)

        kind = record['__rec'][0]

        if kind == 'node':
            self._process_node_record(record)
        elif kind == 'ctx' and process_record_fn is not None:
            process_record_fn(self._expand_record(record))
        elif kind == 'globals':
            self.globals = self._expand_record(record)

        if not kind in self.num_records:
            self.num_records[kind] = 0

        self.num_records[kind] += 1


    def _process_node_record(self, record):
        parent = Node.CALI_INV_ID

        if 'parent' in record:
            parent = int(record['parent'][0])

        self.db.import_node(int(record['id'][0]), int(record['attr'][0]), record['data'][0], parent)


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
