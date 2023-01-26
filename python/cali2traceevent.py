#!/usr/bin/env python3

# Copyright (c) 2022, Lawrence Livermore National Security, LLC.
# See top-level LICENSE file for details.
#
# SPDX-License-Identifier: BSD-3-Clause

# Convert a .cali trace to Google TraceEvent JSON

import json
import time
import sys

import caliperreader


def _get_first_from_list(rec, attribute_list, fallback=0):
    for attr in attribute_list:
        if attr in rec:
            return rec[attr]
    return fallback

def _get_timestamp(rec):
    """Get timestamp from rec and convert to microseconds"""

    timestamp_attributes = {
        "cupti.timestamp"      : 1e-3,
        "rocm.host.timestamp"  : 1e-3,
        "time.offset"          : 1.0,
        "cupti.activity.start" : 1e-3,
        "rocm.starttime"       : 1e-3
    }

    for attr,factor in timestamp_attributes.items():
        if attr in rec:
            return float(rec[attr]) * factor

    return None

def _parse_counter_spec(spec):
    """Parse spec strings in the form
        "group=counter1,counter2,..."
    and return a string->map of strings dict
    """
    res = {}

    if spec is None:
        return res

    pos = spec.find('=')
    grp = spec[:pos]   if pos > 0 else "counter"
    ctr = spec[pos+1:] if pos > 0 else spec

    res[grp] = ctr.split(",")

    return res


class StackFrames:
    """Helper class to build the stackframe dictionary reasonably efficiently"""
    class Node:
        def __init__(self, tree, name, category, parent=None):
            self.parent = parent
            self.name = name
            self.category = category
            self.children = {}
            self.id = len(tree.nodes)
            tree.nodes.append(self)

    def __init__(self):
        self.nodes = []
        self.roots = {}

    def get_stackframe_id(self, path, category):
        if not isinstance(path, list):
            path = [ path ]

        name = path[0]
        key = (category, name)
        if key not in self.roots:
            self.roots[key] = StackFrames.Node(self, name, category)

        node = self.roots[key]
        for name in path[1:]:
            key = (category, name)
            if key not in node.children:
                node.children[key] = StackFrames.Node(self, name, category, node)
            node = node.children[key]

        return node.id

    def get_stackframes(self):
        result = {}

        for node in self.nodes:
            d = dict(name=node.name,category=node.category)
            if node.parent is not None:
                d["parent"] = node.parent.id
            result[node.id] = d

        return result


class CaliTraceEventConverter:
    BUILTIN_PID_ATTRIBUTES = [
        'mpi.rank',
    ]

    BUILTIN_TID_ATTRIBUTES = [
        'omp.thread.id',
        'pthread.id',
    ]

    def __init__(self, cfg):
        self.cfg     = cfg

        self.records = []
        self.reader  = caliperreader.CaliperStreamReader()
        self.rstack  = {}

        self.stackframes = StackFrames()
        self.samples = []

        self.tsync   = {}

        self.counters = self.cfg["counters"]

        self.pid_attributes = self.cfg["pid_attributes"] + self.BUILTIN_PID_ATTRIBUTES
        self.tid_attributes = self.cfg["tid_attributes"] + self.BUILTIN_TID_ATTRIBUTES

        self.skipped = 0
        self.written = 0

    def read(self, filename_or_stream):
        self.reader.read(filename_or_stream, self._process_record)

    def read_and_sort(self, filename_or_stream):
        trace = []

        def insert_into_trace(rec):
            ts = _get_timestamp(rec)
            if ts is None:
                return
            trace.append((ts,rec))

        self.reader.read(filename_or_stream, insert_into_trace)

        trace.sort(key=lambda e : e[0])

        for rec in trace:
            self._process_record(rec[1])

    def write(self, output):
        result = dict(traceEvents=self.records, otherData=self.reader.globals)

        if len(self.stackframes.nodes) > 0:
            result["stackFrames"] = self.stackframes.get_stackframes()
        if len(self.samples) > 0:
            result["samples"] = self.samples

        indent = 1 if self.cfg["pretty_print"] else None
        json.dump(result, output, indent=indent)
        self.written += len(self.records) + len(self.samples)

    def sync_timestamps(self):
        if len(self.tsync) == 0:
            return

        maxts = max(self.tsync.values())
        adjust = { pid: maxts - ts for pid, ts in self.tsync.items() }

        for rec in self.records:
            rec["ts"] += adjust.get(rec["pid"], 0.0)
        for rec in self.samples:
            rec["ts"] += adjust.get(rec["pid"], 0.0)

    def _process_record(self, rec):
        pid  = int(_get_first_from_list(rec, self.pid_attributes))
        tid  = int(_get_first_from_list(rec, self.tid_attributes))

        trec = dict(pid=pid,tid=tid)

        self._process_counters(rec, (pid, tid))

        if "cupti.activity.kind" in rec:
            self._process_cupti_activity_rec(rec, trec)
        elif "rocm.activity" in rec:
            self._process_roctracer_activity_rec(rec, trec)
        elif "umpire.alloc.name" in rec:
            self._process_umpire_rec(rec, trec)
        elif "source.function#cali.sampler.pc" in rec:
            self._process_sample_rec(rec, trec)
            return
        elif "ts.sync" in rec:
            self._process_timesync_rec(rec, pid)
            return
        else:
            keys = list(rec.keys())
            for key in keys:
                if key.startswith("event.begin#"):
                    self._process_event_begin_rec(rec, (pid, tid), key)
                    return
                if key.startswith("event.end#"):
                    self._process_event_end_rec(rec, (pid, tid), key, trec)
                    break
            else:
                self.skipped += 1

        if "name" in trec:
            self.records.append(trec)

    def _process_timesync_rec(self, rec, pid):
        self.tsync[pid] = _get_timestamp(rec)

    def _process_event_begin_rec(self, rec, loc, key):
        attr = key[len("event.begin#"):]
        tst  = _get_timestamp(rec)

        skey = (loc,attr)

        if skey in self.rstack:
            self.rstack[skey].append(tst)
        else:
            self.rstack[skey] = [ tst ]

    def _process_event_end_rec(self, rec, loc, key, trec):
        attr = key[len("event.end#"):]
        btst = self.rstack[(loc,attr)].pop()
        tst  = _get_timestamp(rec)

        self._get_stackframe(rec, trec)

        trec.update(ph="X", name=rec[key], cat=attr, ts=btst, dur=(tst-btst))

    def _process_cupti_activity_rec(self, rec, trec):
        cat  = rec["cupti.activity.kind"]
        tst  = float(rec["cupti.activity.start"])*1e-3
        dur  = float(rec["cupti.activity.duration"])*1e-3
        name = rec.get("cupti.kernel.name", cat)

        trec.update(ph="X", name=name, cat=cat, ts=tst, dur=dur, tid="cuda")

    def _process_roctracer_activity_rec(self, rec, trec):
        cat  = rec["rocm.activity"]
        tst  = float(rec["rocm.starttime"])*1e-3
        dur  = float(rec["rocm.activity.duration"])*1e-3
        name = rec.get("rocm.kernel.name", cat)

        trec.update(ph="X", name=name, cat=cat, ts=tst, dur=dur, tid="rocm")

    def _process_sample_rec(self, rec, trec):
        trec.update(name="sampler", weight=1, ts=_get_timestamp(rec))

        if "cpuinfo.cpu" in rec:
            trec.update(cpu=rec["cpuinfo.cpu"])

        self._get_stackframe(rec, trec)
        self.samples.append(trec)

    def _process_counters(self, rec, loc):
        for grp, counters in self.counters.items():
            args = {}
            for counter in counters:
                if counter in rec:
                    args[counter] = float(rec[counter])
            if len(args) > 0:
                ts = _get_timestamp(rec)
                trec = dict(ph="C", name=grp, pid=loc[0], tid=loc[1], ts=ts, args=args)
                self.records.append(trec)

    def _process_umpire_rec(self, rec, trec):
        name = "Alloc " + rec["umpire.alloc.name"]
        size = float(rec["umpire.alloc.current.size"])
        hwm  = float(rec["umpire.alloc.highwatermark"])
        tst  = _get_timestamp(rec)
        args = { "size": size }

        trec.update(ph="C", name=name, ts=tst, args=args)

    def _get_stackframe(self, rec, trec):
        key = "source.function#callpath.address"
        if key in rec:
            sf = self.stackframes.get_stackframe_id(rec[key], "callstack")
            trec.update(sf=sf)


helpstr = """Usage: cali2traceevent.py 1.cali 2.cali ... [output.json]
Options:
--counters          Specify attributes for "counter" records in the form
                      group=attribute1,attribute2,...
--pretty            Pretty-print output
--sync              Enable time-stamp synchronization
                      Requires timestamp sync records in the input traces
--pid-attributes    List of process ID attributes
--tid-attributes    List of thread ID attributes
"""

def _parse_args(args):
    cfg = {
        "output": sys.stdout,
        "pretty_print": False,
        "sync_timestamps": False,
        "counters": {},
        "tid_attributes": [],
        "pid_attributes": [],
    }

    while args[0].startswith("-"):
        arg = args.pop(0)
        if arg == "--":
            break
        if arg == "--sort":
            # sort is the default now, this switch is deprecated
            pass
        elif arg == "--sync":
            cfg["sync_timestamps"] = True
        elif arg == "--pretty":
            cfg["pretty_print"] = True
        elif arg == "--counters":
            cfg["counters"].update(_parse_counter_spec(args.pop(0)))
        elif arg.startswith("--counters="):
            cfg["counters"].update(_parse_counter_spec(arg[len("--counters="):]))
        elif arg == "--pid-attributes":
            cfg["pid_attributes"] = args.pop(0).split(",")
        elif arg.startswith("--pid-attributes="):
            cfg["pid_attributes"] = arg[len("--pid-attributes="):].split(",")
        elif arg == "--tid-attributes":
            cfg["tid_attributes"] = args.pop(0).split(",")
        elif arg.startswith("--tid-attributes="):
            cfg["tid_attributes"] = arg[len("--tid-attributes="):].split(",")
        elif arg == "--help" or arg == "-h":
            sys.exit(helpstr)
        else:
            sys.exit(f'Unknown argument "{arg}"')

    if len(args) > 1 and args[-1].endswith(".json"):
        cfg["output"] = open(args.pop(), "w")

    return cfg

def main():
    args = sys.argv[1:]

    if len(args) < 1:
        sys.exit(helpstr)

    cfg = _parse_args(args)
    output = cfg["output"]
    converter = CaliTraceEventConverter(cfg)

    read_begin = time.perf_counter()

    for file in args:
        with open(file) as input:
            converter.read_and_sort(input)

    read_end = time.perf_counter()

    if cfg["sync_timestamps"]:
        converter.sync_timestamps()

    converter.write(output)

    write_end = time.perf_counter()

    if output is not sys.stdout:
        output.close()

    wrt = converter.written
    skp = converter.skipped

    rtm = read_end - read_begin
    wtm = write_end - read_end

    print(f"Done. {rtm+wtm:.2f}s ({rtm:.2f}s processing, {wtm:.2f}s write).", file=sys.stderr)
    print(f"{wrt} records written, {skp} skipped.", file=sys.stderr)


if __name__ == "__main__":
    main()
