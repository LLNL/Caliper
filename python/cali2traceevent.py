#!/usr/bin/env python3

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
    """Get timestamp from rec and convert to microseconds
    """

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
    res = {}

    if spec is None:
        return res

    pos = spec.find('=')
    grp = spec[:pos]   if pos > 0 else "counter"
    ctr = spec[pos+1:] if pos > 0 else spec

    res[grp] = ctr.split(",")

    return res


class StackFrames:
    """ Helper class to build the stackframe dictionary reasonably efficiently
    """
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
    PID_ATTRIBUTES = [
        'mpi.rank',
    ]

    TID_ATTRIBUTES = [
        'omp.thread.id',
        'pthread.id',
    ]

    def __init__(self, counters=None):
        self.records = []
        self.reader  = caliperreader.CaliperStreamReader()
        self.rstack  = {}

        self.stackframes = StackFrames()
        self.samples = []

        self.counters = counters

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

    def write(self, output, pretty_print=False):
        result = dict(traceEvents=self.records, otherData=self.reader.globals)

        if len(self.stackframes.nodes) > 0:
            result["stackFrames"] = self.stackframes.get_stackframes()
        if len(self.samples) > 0:
            result["samples"] = self.samples

        json.dump(result, output, indent=1 if pretty_print else None)
        self.written += len(self.records)

    def _process_record(self, rec):
        pid  = int(_get_first_from_list(rec, self.PID_ATTRIBUTES))
        tid  = int(_get_first_from_list(rec, self.TID_ATTRIBUTES))

        trec = dict(pid=pid,tid=tid)

        self._process_counters(rec, (pid, tid))

        if "cupti.activity.kind" in rec:
            self._process_cupti_activity_rec(rec, trec)
        elif "rocm.activity" in rec:
            self._process_roctracer_activity_rec(rec, trec)
        elif "source.function#cali.sampler.pc" in rec:
            self._process_sample_rec(rec, trec)
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

        if "name" in trec:
            self.records.append(trec)
        else:
            self.skipped += 1

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

    def _get_stackframe(self, rec, trec):
        key = "source.function#callpath.address"
        if key in rec:
            sf = self.stackframes.get_stackframe_id(rec[key], "callstack")
            trec.update(sf=sf)


helpstr = """Usage: cali2traceevent.py 1.cali 2.cali ... [output.json]
Options:
--counters      Specify attributes for "counter" records in the form
                    group=attribute1,attribute2,...
--pretty        Pretty-print output
--sort          Sort the trace before processing.
                Enable this when encountering stack errors.
"""

def _parse_args(args):
    cfg = {
        "output": sys.stdout,
        "sort_the_trace": False,
        "pretty_print": False,
        "counters": {}
    }

    while args[0].startswith("-"):
        arg = args.pop(0)
        if arg == "--":
            break
        if arg == "--sort":
            cfg["sort_the_trace"] = True
        elif arg == "--pretty":
            cfg["pretty_print"] = True
        elif arg == "--counters":
            cfg["counters"].update(_parse_counter_spec(args.pop(0)))
        elif arg.startswith("--counters="):
            cfg["counters"].update(_parse_counter_spec(arg[len("--counters="):]))
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
    converter = CaliTraceEventConverter(cfg["counters"])

    read_begin = time.perf_counter()

    for file in args:
        with open(file) as input:
            if cfg["sort_the_trace"]:
                converter.read_and_sort(input)
            else:
                converter.read(input)

    read_end = time.perf_counter()
    converter.write(output, cfg["pretty_print"])
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
