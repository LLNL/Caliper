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
    """

    def __init__(self):
        self.db = MetadataDB()
        self.globals = {}


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
        """ Return the list of attribute names.

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


    def _process_node_record(self, record):
        parent = Node.CALI_INV_ID

        if 'parent' in record:
            parent = int(record['parent'][0])

        self.db.import_node(int(record['id'][0]), int(record['attr'][0]), record.get('data', [""])[0], parent)


    def _expand_record(self, record):
        result = {}

        if 'ref' in record:
            for node_id in record['ref']:
                result.update(self.db.nodes[int(node_id)].expand())

        if 'attr' in record and 'data' in record:
            for attr_id, val in zip(record['attr'], record['data']):
                attr = self.db.attributes_by_id[int(attr_id)]
                if not attr.is_hidden():
                    result[attr.name()] = val

        return result


def _read_cali_record(line):
    """ Splits comma-separated Caliper record line into constituent elements

        A .cali line has the form

          "__rec=ctx,ref=42=4242,attr=1=2=3,data=11=22=33"

        This function splits this into a key-list dict like so:

          {
            "__rec" : [ "ctx" ],
            "ref"   : [ "42", "4242" ],
            "attr"  : [ "1", "2", "3" ],
            "data"  : [ "11", "22", "33" ]
          }

        We need to deal with possibly escaped characters in strings so we
        can't just use str.split().
    """

    result  = {}
    entry   = []
    string  = ""

    iterator = iter(line.strip())

    for c in iterator:
        if c == '\\':
            c = next(iterator)
            if c == 'n':
                string += '\n'
            else:
                string += c
        elif c == ',':
            entry.append(string)
            result[entry[0]] = entry[1:]
            string = ""
            entry  = []
        elif c == '=':
            entry.append(string)
            string = ""
        else:
            string += c

    if len(string) > 0:
        entry.append(string)
        result[entry[0]] = entry[1:]

    return result
