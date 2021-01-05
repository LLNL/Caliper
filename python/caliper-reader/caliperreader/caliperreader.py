# Copyright (c) 2020, Lawrence Livermore National Security, LLC.
# See top-level LICENSE file for details.
#
# SPDX-License-Identifier: BSD-3-Clause

from .caliperstreamreader import CaliperStreamReader
from .metadatadb import Attribute, Node, MetadataDB


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

        sr = CaliperStreamReader()

        sr.read(filename_or_stream, self._add_record)

        self.db = sr.db
        self.globals = sr.globals
        self.num_records = sr.num_records


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


    def _add_record(self, rec):
        self.records.append(rec)


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

    reader = CaliperStreamReader()
    reader.read(filename_or_stream, None)

    return reader.globals