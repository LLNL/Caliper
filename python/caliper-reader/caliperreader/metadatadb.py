# Copyright (c) 2020, Lawrence Livermore National Security, LLC.
# See top-level LICENSE file for details.
#
# SPDX-License-Identifier: BSD-3-Clause

class Node:
    """ A Caliper metadata tree node. """

    CALI_INV_ID = 0xFFFFFFFFFFFFFFFF

    def __init__(self, metadb, id, attribute_id, data):
        self.parent       = None
        self.children     = []

        self.id           = id
        self.attribute_id = attribute_id
        self.data         = data

        self.metadb       = metadb


    def append(self, child):
        """ Append a child node to this node.
        """

        child.parent = self
        self.children.append(child)

    def get(self, attribute_id_or_name):
        """ Iterate through parents and return the data element for the first
        node with the given attribute, or None when no such element exists.
        """

        if isinstance(attribute_id_or_name, str):
            if not attribute_id_or_name in self.metadb.attributes:
                return None

            attribute_id = self.metadb.attributes[attribute_id_or_name].id()
        else:
            attribute_id = attribute_id_or_name

        node = self
        while node is not None and node.attribute_id != attribute_id:
            node = node.parent

        return node.data if node else None

    def attribute(self):
        """ Return the Caliper attribute object for this node.
        """
        return Attribute(self.metadb.nodes[self.attribute_id])


class Attribute:
    """ A Caliper attribute key.

    Provides information about Caliper attribute keys.
    """

    attr_attribute_id =   8
    type_attribute_id =   9
    prop_attribute_id =  10

    CALI_ATTR_ASVALUE =   1
    CALI_ATTR_NESTED  = 256

    def __init__(self, node):
        self.node = node

    def name(self):
        """ Return the name of the Caliper attribute. """
        return self.node.data

    def id(self):
        """ Return the node ID of the Caliper attribute. """
        return self.node.id

    def get(self, attribute):
        """ Return metadata value for this attribute.

        Arguments:
            attribute : string or integer
                Metadata attribute name or ID

        Returns:
            (object):
                The metadata value.
        """

        return self.node.get(attribute)

    def attribute_type(self):
        """ Return the name of the attribute's Caliper data type. """

        return self.node.get(self.type_attribute_id)[1]

    def is_nested(self):
        """ Does this attribute have the Caliper "nested" property?

        Attributes with the "nested" property define the hierarchy of Caliper
        begin/end annotations.
        """

        prop = self.node.get(self.prop_attribute_id)
        return (int(prop) & self.CALI_ATTR_NESTED) != 0

    def is_value(self):
        """ Does this attribute have the "as_value" property?

        Attributes with the "as_value" property are stored directly in
        snapshot records.
        """

        prop = self.node.get(self.prop_attribute_id)
        return (int(prop) & self.CALI_ATTR_ASVALUE) != 0


class MetadataDB:
    """ The Caliper metadata tree """

    def __init__(self):
        # The Caliper attribute type definitions
        self.types = [
            ( 0, 'inv'    ),
            ( 1, 'usr'    ),
            ( 2, 'int'    ),
            ( 3, 'uint'   ),
            ( 4, 'string' ),
            ( 5, 'addr'   ),
            ( 6, 'double' ),
            ( 7, 'bool'   ),
            ( 8, 'type'   ),
            ( 9, 'ptr'    )
        ]

        #   Initialize the node list with the pre-defined bootstrap nodes.
        # Use a dict for nodes because there can be gaps in the node list.
        self.nodes = {
             0: Node(self,  0, 9, self.types[1]),
             1: Node(self,  1, 9, self.types[2]),
             2: Node(self,  2, 9, self.types[3]),
             3: Node(self,  3, 9, self.types[4]),
             4: Node(self,  4, 9, self.types[5]),
             5: Node(self,  5, 9, self.types[6]),
             6: Node(self,  6, 9, self.types[7]),
             7: Node(self,  7, 9, self.types[8]),
             8: Node(self,  8, 8, 'cali.attribute.name'),
             9: Node(self,  9, 8, 'cali.attribute.type'),
            10: Node(self, 10, 8, 'cali.attribute.prop'),
            11: Node(self, 11, 9, self.types[9])
        }

        # Append bootstrap node children to parents
        self.nodes[3].append(self.nodes[ 8])
        self.nodes[7].append(self.nodes[ 9])
        self.nodes[1].append(self.nodes[10])

        # Initialize the attributes dict
        self.attributes = {
            'cali.attribute.name': Attribute(self.nodes[ 8]),
            'cali.attribute.type': Attribute(self.nodes[ 9]),
            'cali.attribute.prop': Attribute(self.nodes[10])
        }


    def import_node(self, node_id, attribute_id, data, parent_id = Node.CALI_INV_ID):
        node = Node(self, node_id, attribute_id, data)
        self.nodes[node_id] = node

        if parent_id in self.nodes:
            self.nodes[parent_id].append(node)

        if attribute_id == Attribute.attr_attribute_id:
            self.attributes[data] = Attribute(node)
