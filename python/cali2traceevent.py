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


class CaliTraceEventConverter:
    PID_ATTRIBUTES = [
        'mpi.rank',
    ]

    TID_ATTRIBUTES = [
        'omp.thread.id',
        'pthread.id',
    ]

    def __init__(self):
        self.records = []
        self.reader  = caliperreader.CaliperStreamReader()
        self.rstack  = {}

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
        json.dump(self.records, output)
        self.written += len(self.records)

    def _process_record(self, rec):
        pid  = int(_get_first_from_list(rec, self.PID_ATTRIBUTES))
        tid  = int(_get_first_from_list(rec, self.TID_ATTRIBUTES))

        trec = dict(pid=pid,tid=tid)

        if "cupti.activity.kind" in rec:
            self._process_cupti_activity_rec(rec, trec)
        elif "rocm.activity" in rec:
            self._process_roctracer_activity_rec(rec, trec)
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

        # if not self.reader.attribute(attr).is_nested():
        #     return

        tst  = _get_timestamp(rec)

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

helpstr = """Usage: cali2traceevent.py 1.cali 2.cali ... [output.json]
Options:
--sort      Sort the trace before processing.
            Enable this when encountering stack errors.
"""

def main():
    args = sys.argv[1:]

    if len(args) < 1:
        sys.exit(helpstr)

    output = sys.stdout
    sort_the_trace = False

    while args[0].startswith("-"):
        arg = args.pop(0)
        if arg == "--":
            break
        if arg == "--sort":
            sort_the_trace = True
        elif arg == "--help":
            sys.exit(helpstr)
        else:
            sys.exit(f'Unknown argument "{arg}"')

    if len(args) > 1 and args[-1].endswith(".json"):
        output = open(args.pop(), "w")

    converter = CaliTraceEventConverter()

    read_begin = time.perf_counter()

    for file in args:
        with open(file) as input:
            if sort_the_trace:
                converter.read_and_sort(input)
            else:
                converter.read(input)

    read_end = time.perf_counter()

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
