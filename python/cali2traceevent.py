#!/usr/bin/env python3

# Convert a .cali trace to Google TraceEvent JSON

import caliperreader

import json
import sys


def _get_first_from_list(rec, attribute_list, fallback=0):
    for a in attribute_list:
        if a in rec:
            return rec[a]
    return fallback


class CaliTraceEventConverter:
    PID_ATTRIBUTES = [
        'mpi.rank'
    ]

    TID_ATTRIBUTES = [
        'omp.thread.id',
        'pthread.id'
    ]

    TS_ATTRIBUTES = [
        'cupti.timestamp',
        'cupti.activity.start',
        'cupti.activity.end',
        'time.offset'
    ]

    def __init__(self):
        self.records = []
        self.reader  = caliperreader.CaliperStreamReader()

        self.skipped = 0
        self.written = 0

    def read(self, filename_or_stream):
        self.reader.read(filename_or_stream, self._process_record)

    def write(self, output):
        json.dump(self.records, output)
        self.written += len(self.records)

    def _process_record(self, rec):
        pid  = int(_get_first_from_list(rec, self.PID_ATTRIBUTES))
        tid  = int(_get_first_from_list(rec, self.TID_ATTRIBUTES))

        ts   = float(_get_first_from_list(rec, self.TS_ATTRIBUTES, None))
        ts  *= 1e-3

        if ts is None:
            self.skipped += 1
            return

        trec = dict(pid=pid,tid=tid,ts=ts)
        keys = list(rec.keys())

        for k in keys:
            if k.startswith("event.begin#"):
                attr = k[len("event.begin#"):]
                trec.update(ph="B", name=rec[k], cat=attr)
                break
            elif k.startswith("event.end#"):
                attr = k[len("event.end#"):]
                trec.update(ph="E", name=rec[k], cat=attr)
                break
        
        if "name" in trec:
            self.records.append(trec)
        else:
            self.skipped += 1


def main():
    args = sys.argv[1:]

    if len(args) < 1:
        sys.exit("Usage: cali2traceevent.py 1.cali 2.cali ... [output.json]")

    output = sys.stdout

    if len(args) > 1 and args[-1].endswith(".json"):
        output = open(args.pop(), "w")
    
    converter = CaliTraceEventConverter()

    for f in args:
        with open(f) as input:
            converter.read(f)

    converter.write(output)

    if output is not sys.stdout:
        output.close()

    w = converter.written
    s = converter.skipped

    print("{} records written, {} skipped.".format(w, s), file=sys.stderr)


if __name__ == "__main__":
    main()