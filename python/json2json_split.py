#!/usr/bin/env python3

# Copyright (c) 2022, Lawrence Livermore National Security, LLC.
# See top-level LICENSE file for details.
#
# SPDX-License-Identifier: BSD-3-Clause

# Convert the "simple" Caliper json format to Caliper json-split json format

import json
import sys

class Tree:
    """ Helper class to build the json-split node list reasonably efficiently
    """
    class Node:
        def __init__(self, tree, data, parent=None):
            self.parent = parent
            self.data = data
            self.children = {}
            self.id = len(tree.nodes)
            tree.nodes.append(self)

    def __init__(self):
        self.nodes = []
        self.roots = {}

    def get_node_id(self, column, path):
        val  = (column, path[0])
        if val not in self.roots:
            self.roots[val] = Tree.Node(self, val)

        node = self.roots[val]
        for name in path[1:]:
            val = (column, name)
            if val not in node.children:
                node.children[val] = Tree.Node(self, val, node)
            node = node.children[val]

        return node.id

    def get_nodelist(self):
        result = []

        for node in self.nodes:
            d = { "column": node.data[0], "label": node.data[1] }
            if node.parent is not None:
                d["parent"] = node.parent.id
            result.append(d)

        return result


def json_to_json_split(records):
    coldict = {}

    for rec in records:
        for k in rec.keys():
            if not k in coldict:
                coldict[k] = { "is_value": not isinstance(rec[k], str) }

    columns = list(coldict.keys())
    column_metadata = [ coldict[k] for k in columns ]

    data  = []
    tree  = Tree()

    for rec in records:
        entry = []
        for col in columns:
            if not col in rec:
                entry.append(None)
            elif coldict[col]["is_value"] == False:
                entry.append(tree.get_node_id(col, rec[col].split("/")))
            else:
                entry.append(rec[col])
        data.append(entry)

    return {
        "data": data,
        "columns": columns,
        "column_metadata": column_metadata,
        "nodes": tree.get_nodelist()
    }


def main():
    args = sys.argv[1:]

    if len(args) < 1 or len(args) > 2:
        sys.exit("Usage: json2json_split.py input.json [output.json]")

    with open(args[0]) as input:
        records = json.load(input)

    output = open(args[1], "w") if len(args) > 1 else sys.stdout
    json.dump(json_to_json_split(records), output)

    if output is not sys.stdout:
        output.close()


if __name__ == "__main__":
    main()